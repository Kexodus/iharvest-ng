#include "Manager.h"

#include "Forms.h"
#include "Log.h"
#include "Placements.h"

namespace ihng
{
	namespace
	{
		constexpr std::size_t kCandidateCacheSize = 8;      // like the original's 8 finder workers
		constexpr float       kIgnoreAfterHarvest = 30.0f;  // seconds a harvested target is skipped
		constexpr float       kIgnoreAfterFailure = 10.0f;  // seconds a target is skipped after a failed trip
		constexpr float       kSpawnDistance = 192.0f;      // original: 192 * speedMult in front of the caster
		constexpr float       kSpawnHeightFactor = 0.6f;
		constexpr float       kDualSpawnSpread = 48.0f;   // sideways offset of each wisp in a dual-cast pair
		constexpr RE::FormID    kLeftHandSlotID = 0x13F43;  // vanilla LeftHand equip slot

		RE::Actor* AsActor(RE::TESObjectREFR* a_ref)
		{
			return a_ref ? a_ref->As<RE::Actor>() : nullptr;
		}

		bool IsHarvestStaff(const RE::TESForm* a_form)
		{
			const auto& forms = Forms::Get();
			const auto* weap = a_form ? a_form->As<RE::TESObjectWEAP>() : nullptr;
			return weap && (weap == forms.harvestStaff || (forms.isHarvestStaff && weap->HasKeyword(forms.isHarvestStaff)));
		}
	}

	Manager& Manager::GetSingleton() noexcept
	{
		static Manager singleton;
		return singleton;
	}

	void Manager::QueueTask(std::function<void()> a_task)
	{
		std::scoped_lock l(_queueLock);
		_tasks.push_back(std::move(a_task));
	}

	void Manager::WatchActor(RE::ActorHandle a_actor)
	{
		std::scoped_lock l(_queueLock);
		_pendingWatch.push_back(a_actor);
	}

	void Manager::RunQueuedTasks()
	{
		std::vector<std::function<void()>> tasks;
		std::vector<RE::ActorHandle>       watch;
		{
			std::scoped_lock l(_queueLock);
			tasks.swap(_tasks);
			watch.swap(_pendingWatch);
		}
		for (auto& handle : watch) {
			if (std::ranges::find(_watched, handle) == _watched.end()) {
				_watched.push_back(handle);
			}
		}
		for (auto& task : tasks) {
			task();
		}
	}

	RE::ActiveEffect* Manager::FindActiveEffect(RE::Actor* a_actor, RE::EffectSetting* a_effect)
	{
		if (!a_actor || !a_effect) {
			return nullptr;
		}
		auto* target = a_actor->AsMagicTarget();
		auto* list = target ? target->GetActiveEffectList() : nullptr;
		if (!list) {
			return nullptr;
		}
		for (auto* effect : *list) {
			if (effect && effect->GetBaseObject() == a_effect &&
				effect->flags.none(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled)) {
				return effect;
			}
		}
		return nullptr;
	}

	void Manager::ApplyConfigChanges()
	{
		auto& settings = Settings::GetSingleton();
		const auto rev = settings.Revision();
		if (rev == _configRevision) {
			return;
		}
		_configRevision = rev;
		_config = settings.Get();
		log::SetLevel(_config.logLevel);

		const harvest::Rules rules{ _config.harvestFood, _config.harvestHearthfire, _config.harvestCritters, _config.harvestArrows };
		if (rules != _rules) {
			_rules = rules;
			harvest::ClearCache();
			for (auto& s : _sessions) {
				s.cache.clear();
			}
		}
	}

	void Manager::Update()
	{
		const auto frameStart = std::chrono::steady_clock::now();
		ApplyConfigChanges();
		RunQueuedTasks();

		if (!Forms::IsReady()) {
			return;
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->Is3DLoaded() || !player->GetParentCell()) {
			return;  // main menu, loading screen
		}
		if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
			return;
		}
		const float dt = std::clamp(RE::GetSecondsSinceLastFrame(), 0.0f, 0.25f);
		if (dt <= 0.0f) {
			return;
		}
		_clock += dt;

		if (!_deleteOnNextUpdate.empty()) {
			DeleteOrphansNow();
		}

		UpdateSessions(dt);
		UpdateWisps(dt);
		if (!_pendingKeyframe.empty()) {
			UpdatePendingKeyframes(dt);
		}

		if (!_ignoredUntil.empty()) {
			std::erase_if(_ignoredUntil, [this](const auto& kv) { return kv.second <= _clock; });
		}

		const float frameMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - frameStart).count();
		std::scoped_lock l(_statsLock);
		_stats.activeWisps = static_cast<std::uint32_t>(_wisps.size());
		_stats.activeCasters = static_cast<std::uint32_t>(_sessions.size());
		_stats.learnedTypes = static_cast<std::uint32_t>(harvest::LearnedCount());
		_stats.examinedTypes = static_cast<std::uint32_t>(harvest::ExaminedCount());
		_stats.lastFrameMs = frameMs;
	}

	// ---- Sessions ---------------------------------------------------------------------------------

	void Manager::SyncSession(RE::Actor* a_actor, bool a_staff, RE::ActiveEffect* a_effect)
	{
		const auto handle = a_actor->GetHandle();
		auto it = std::ranges::find_if(_sessions, [&](const Session& s) { return s.alive && s.staff == a_staff && s.caster == handle; });
		if (a_effect && it == _sessions.end()) {
			StartSession(a_actor, a_staff, a_effect);
		} else if (!a_effect && it != _sessions.end()) {
			it->alive = false;
		}
	}

	void Manager::StartSession(RE::Actor* a_actor, bool a_staff, RE::ActiveEffect* a_effect)
	{
		auto* av = a_actor->AsActorValueOwner();
		const auto alteration = static_cast<std::int32_t>(av->GetActorValue(RE::ActorValue::kAlteration));
		const auto alchemy = static_cast<std::int32_t>(av->GetActorValue(RE::ActorValue::kAlchemy));

		bool dual = false;
		if (!a_staff) {
			dual = a_effect->flags.any(RE::ActiveEffect::Flag::kDual);
			bool graphDual = false;
			if (!dual && a_actor->GetGraphVariableBool("IsCastingDual", graphDual)) {
				dual = graphDual;
			}
		}

		Session s;
		s.caster = a_actor->GetHandle();
		s.staff = a_staff;
		s.tuning = a_staff ? &core::kStaffTuning : &core::kSpellTuning;
		const auto params = core::ComputeCastParams(*s.tuning, alteration, alchemy, dual);
		s.clock = core::SpawnClock(params);
		s.radius = params.radius * _config.radiusMult;
		s.cost = core::ComputeWispCost(*s.tuning, a_staff ? _config.staffChargePerWisp : _config.magickaCostPerWisp,
			av->GetActorValue(RE::ActorValue::kAlterationModifier));
		if (a_staff) {
			s.chargeAV = IsHarvestStaff(a_actor->GetEquippedObject(false)) ? RE::ActorValue::kRightItemCharge : RE::ActorValue::kLeftItemCharge;
		}
		if (auto* perk = Forms::Get().alterationDualCasting; !a_staff && dual && _config.dualCastPerkDoubleWisps && perk && a_actor->HasPerk(perk)) {
			s.wispsPerRefire = 2;
		}

		s.startedAt = _clock;
		logger::info("Harvest {} started by {:08X} ({}): alt={} alch={} dual={} wisps/refire={} radius={:.0f} minDelay={}us accel={:.3f} cost={:.2f}",
			a_staff ? "staff" : "cast", a_actor->GetFormID(), a_actor->GetName(), alteration, alchemy, dual, s.wispsPerRefire, s.radius,
			params.minDelayUs, params.accel, s.cost);
		_sessions.push_back(std::move(s));
	}

	void Manager::UpdateSessions(float a_dt)
	{
		const auto& forms = Forms::Get();
		auto        check = [&](RE::Actor* a_actor) {
            SyncSession(a_actor, false, FindActiveEffect(a_actor, forms.harvestEffect));
            SyncSession(a_actor, true, FindActiveEffect(a_actor, forms.harvestStaffEffect));
		};

		check(RE::PlayerCharacter::GetSingleton());

		std::erase_if(_watched, [&](const RE::ActorHandle& h) {
			const auto actor = h.get();
			if (!actor || actor->IsDead() || !actor->Is3DLoaded() || !_config.allowNPCCasters) {
				return true;
			}
			check(actor.get());
			const bool stillCasting = std::ranges::any_of(_sessions, [&](const Session& s) { return s.alive && s.caster == h; });
			return !stillCasting;
		});

		for (auto& s : _sessions) {
			if (!s.alive) {
				continue;
			}
			const auto caster = s.caster.get();
			if (!caster || caster->IsDead() || !caster->Is3DLoaded()) {
				s.alive = false;
				continue;
			}
			if (caster.get() != RE::PlayerCharacter::GetSingleton() && !_config.allowNPCCasters) {
				s.alive = false;
				continue;
			}
			if (s.clock.Tick(a_dt)) {
				s.clock.Report(Attempt(s, caster.get()));
			}
		}
		std::erase_if(_sessions, [this](const Session& s) {
			if (s.alive) {
				return false;
			}
			logger::info("Harvest {} ended after {:.1f}s: {} wisp(s) sent, {} in flight", s.staff ? "staff" : "cast",
				_clock - s.startedAt, s.sent, _wisps.size());
			return true;
		});
	}

	bool Manager::IsClaimedOrIgnored(RE::FormID a_id) const
	{
		if (_claimed.contains(a_id)) {
			return true;
		}
		const auto it = _ignoredUntil.find(a_id);
		return it != _ignoredUntil.end() && it->second > _clock;
	}

	void Manager::Rescan(Session& a_session, RE::Actor* a_caster)
	{
		const auto start = std::chrono::steady_clock::now();

		struct Candidate
		{
			float              dist2;
			RE::TESObjectREFR* ref;
		};
		std::vector<Candidate> found;
		found.reserve(64);

		const auto origin = a_caster->GetPosition();
		const bool owned = _config.respectOwnership;
		RE::TES::GetSingleton()->ForEachReferenceInRange(a_caster, a_session.radius, [&](RE::TESObjectREFR* a_ref) {
			if (!a_ref || a_ref == a_caster || a_ref->GetFormType() == RE::FormType::ActorCharacter) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			const auto kind = harvest::Classify(a_ref, _rules);
			if (kind == harvest::Kind::kNone || IsClaimedOrIgnored(a_ref->GetFormID()) || !harvest::IsAvailable(a_ref, kind, a_caster, owned)) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			found.push_back({ origin.GetSquaredDistance(a_ref->GetPosition()), a_ref });
			return RE::BSContainer::ForEachResult::kContinue;
		});

		const auto n = std::min(found.size(), kCandidateCacheSize);
		std::partial_sort(found.begin(), found.begin() + n, found.end(), [](const Candidate& a, const Candidate& b) { return a.dist2 < b.dist2; });
		a_session.cache.clear();
		for (std::size_t i = n; i-- > 0;) {
			a_session.cache.push_back(found[i].ref->GetHandle());  // nearest ends up last (popped first)
		}

		const float ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
		logger::trace("Scan: {} candidates within {:.0f} in {:.3f} ms", found.size(), a_session.radius, ms);
		std::scoped_lock l(_statsLock);
		_stats.lastScanMs = ms;
	}

	RE::TESObjectREFR* Manager::NextTarget(Session& a_session, RE::Actor* a_caster, harvest::Kind& a_kindOut)
	{
		for (int pass = 0; pass < 2; ++pass) {
			while (!a_session.cache.empty()) {
				const auto ref = a_session.cache.back().get();
				a_session.cache.pop_back();
				if (!ref || IsClaimedOrIgnored(ref->GetFormID())) {
					continue;
				}
				const auto kind = harvest::Classify(ref.get(), _rules);
				if (harvest::IsAvailable(ref.get(), kind, a_caster, _config.respectOwnership)) {
					a_kindOut = kind;
					return ref.get();
				}
			}
			if (pass == 0) {
				Rescan(a_session, a_caster);
			}
		}
		return nullptr;
	}

	core::AttemptResult Manager::Attempt(Session& a_session, RE::Actor* a_caster)
	{
		// The first wisp of a refire behaves exactly like the original; extra wisps (Dual Casting perk)
		// are only sent if the first one went out and they can be paid for.
		auto result = core::AttemptResult::kNeutral;
		for (std::uint32_t i = 0; i < a_session.wispsPerRefire && a_session.alive; ++i) {
			const auto r = DispatchOne(a_session, a_caster, i);
			if (i == 0) {
				result = r;
			}
			if (r != core::AttemptResult::kDispatched) {
				break;
			}
		}
		return result;
	}

	core::AttemptResult Manager::DispatchOne(Session& a_session, RE::Actor* a_caster, std::uint32_t a_index)
	{
		const auto& forms = Forms::Get();
		auto*       av = a_caster->AsActorValueOwner();

		// Pay-as-you-go, like the original: the cast itself is free, every wisp costs. Only the first
		// wisp of a refire ends the cast when it can't be paid for; an extra one is simply skipped.
		const float available = av->GetActorValue(a_session.staff ? a_session.chargeAV : RE::ActorValue::kMagicka);
		if (available < a_session.cost) {
			if (a_index == 0) {
				if (a_session.staff) {
					a_caster->RemoveSpell(forms.harvestStaffAbility);
				} else {
					a_caster->InterruptCast(false);
				}
				a_session.alive = false;
			}
			return core::AttemptResult::kNeutral;
		}

		if (_wisps.size() >= static_cast<std::size_t>(_config.maxWisps)) {
			return core::AttemptResult::kAtCap;
		}

		harvest::Kind kind{};
		auto*         target = NextTarget(a_session, a_caster, kind);
		if (!target) {
			return core::AttemptResult::kNothingFound;
		}

		// Spawn point: 192 units in front of the caster (further when running; behind for negative
		// multipliers) - same formula as the original's SetTargets2/MoveTo.
		float speed = 100.0f;
		if (_config.spawnDistanceMult > 0.0f && a_caster->IsRunning()) {
			speed = av->GetActorValue(RE::ActorValue::kSpeedMult) * 2.0f;
		}
		speed = std::clamp(speed * _config.spawnDistanceMult, -300.0f, 300.0f);
		const float mult = speed / 100.0f;

		Wisp::LaunchParams params;
		params.caster = a_caster;
		params.target = target;
		params.kind = kind;
		params.casterHeading = a_caster->GetAngleZ();
		params.spawnBehind = mult < 0.0f;
		params.spawn = a_caster->GetPosition();
		params.spawn.x += kSpawnDistance * mult * std::sin(params.casterHeading);
		params.spawn.y += kSpawnDistance * mult * std::cos(params.casterHeading);
		params.spawn.z += a_caster->GetHeight() * kSpawnHeightFactor;
		if (a_session.wispsPerRefire > 1) {
			// Fan a dual-cast pair out sideways, one from each hand.
			const float side = (a_index % 2 == 0 ? -1.0f : 1.0f) * kDualSpawnSpread;
			params.spawn.x += side * std::cos(params.casterHeading);
			params.spawn.y -= side * std::sin(params.casterHeading);
		}

		auto wisp = Wisp::Launch(params, _config);
		if (!wisp) {
			return core::AttemptResult::kNeutral;
		}
		_claimed.insert(target->GetFormID());
		_wisps.push_back(std::move(wisp));
		++a_session.sent;
		{
			std::scoped_lock l(_statsLock);
			++_stats.totalSent;
			++_stats.sessionSent;
		}

		if (a_caster == RE::PlayerCharacter::GetSingleton() && _config.xpPerWisp > 0.0f) {
			RE::PlayerCharacter::GetSingleton()->AddSkillExperience(RE::ActorValue::kAlteration, _config.xpPerWisp);
		}
		if (a_session.cost > 0.0f) {
			av->DamageActorValue(a_session.staff ? a_session.chargeAV : RE::ActorValue::kMagicka, a_session.cost);
		}
		return core::AttemptResult::kDispatched;
	}

	// ---- Wisps ------------------------------------------------------------------------------------

	void Manager::Release(const Wisp& a_wisp)
	{
		_claimed.erase(a_wisp.TargetID());
		_ignoredUntil[a_wisp.TargetID()] = static_cast<float>(_clock) + (a_wisp.Harvested() ? kIgnoreAfterHarvest : kIgnoreAfterFailure);
	}

	void Manager::UpdateWisps(float a_dt)
	{
		for (auto it = _wisps.begin(); it != _wisps.end();) {
			auto& wisp = **it;
			if (wisp.Update(a_dt, _config)) {
				++it;
				continue;
			}
			Release(wisp);
			if (wisp.Harvested()) {
				std::scoped_lock l(_statsLock);
				++_stats.totalHarvested;
				++_stats.sessionHarvested;
			}
			it = _wisps.erase(it);
		}
	}

	// ---- Staff of Harvest -------------------------------------------------------------------------

	void Manager::OnStaffCast(RE::Actor* a_caster)
	{
		// IH_StaffControllerScript: casting the staff toggles the harvesting ability on and off.
		const auto& forms = Forms::Get();
		if (!a_caster || !forms.harvestStaffAbility) {
			return;
		}
		if (FindActiveEffect(a_caster, forms.harvestStaffEffect)) {
			a_caster->RemoveSpell(forms.harvestStaffAbility);
		} else {
			a_caster->RemoveSpell(forms.harvestStaffAbility);
			a_caster->AddSpell(forms.harvestStaffAbility);
			if (a_caster != RE::PlayerCharacter::GetSingleton()) {
				WatchActor(a_caster->GetHandle());
			}
		}
	}

	void Manager::OnStaffEquipChanged(RE::Actor* a_actor, bool a_equipped)
	{
		// IH_HarvestStaffScript: no dual-wielding harvest staves; unequipping ends harvesting.
		const auto& forms = Forms::Get();
		if (!a_actor) {
			return;
		}
		if (!a_equipped) {
			if (!IsHarvestStaff(a_actor->GetEquippedObject(false)) && !IsHarvestStaff(a_actor->GetEquippedObject(true))) {
				a_actor->RemoveSpell(forms.harvestStaffAbility);
			}
			return;
		}
		auto* left = a_actor->GetEquippedObject(true);
		auto* right = a_actor->GetEquippedObject(false);
		if (IsHarvestStaff(left) && IsHarvestStaff(right)) {
			auto* slot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(kLeftHandSlotID);
			RE::ActorEquipManager::GetSingleton()->UnequipObject(a_actor, left->As<RE::TESBoundObject>(), nullptr, 1, slot);
		}
	}

	// ---- Spell tomes and orphan cleanup -----------------------------------------------------------

	void Manager::OnReferenceAttached(RE::TESObjectREFR* a_ref)
	{
		const auto& forms = Forms::Get();
		auto*       base = a_ref ? a_ref->GetBaseObject() : nullptr;
		if (!base) {
			return;
		}

		if (base == forms.spellTome) {
			// IH_SpellbookScript: tomes are placed generously by hand, so once you know the spell most
			// of them quietly vanish when you come across them (each one is only ever rolled once).
			if (!_processedTomes.insert(a_ref->GetFormID()).second) {
				return;
			}
			if (forms.spellTome->IsRead() && !a_ref->IsDisabled()) {
				std::uniform_real_distribution<float> roll(0.0f, 1.0f);
				if (roll(_rng) < _config.readTomeDespawnChance) {
					a_ref->Disable();
					logger::debug("Spell tome {:08X} despawned (spell already known)", a_ref->GetFormID());
				}
			}
			return;
		}

		if (base == forms.wisp || base == forms.wispSilent || base == forms.spawnPoof) {
			const auto id = a_ref->GetFormID();
			// Poofs are no longer created, so any poof reference is a leftover from an older build.
			const bool tracked = base != forms.spawnPoof && std::ranges::any_of(_wisps, [id](const auto& w) { return w->RefID() == id; });
			if (!tracked) {
				logger::debug("Deleting orphaned temp ref {:08X}", id);
				DeleteTempRef(a_ref);
			}
		}
	}

	void Manager::DeleteOrphansNow()
	{
		const auto& forms = Forms::Get();
		for (const auto id : _deleteOnNextUpdate) {
			auto* ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(id);
			auto* base = ref ? ref->GetBaseObject() : nullptr;
			if (base && (base == forms.wisp || base == forms.wispSilent || base == forms.spawnPoof)) {
				DeleteTempRef(ref);
			}
		}
		logger::info("Cleaned up {} wisp reference(s) that were saved mid-flight", _deleteOnNextUpdate.size());
		_deleteOnNextUpdate.clear();
	}

	void Manager::Place(std::size_t a_index)
	{
		const auto& def = placements::All()[a_index];
		const auto& r = placements::Get(a_index);
		if (!r.base || !r.cell) {
			return;
		}
		const auto handle = RE::TESDataHandler::GetSingleton()->CreateReferenceAtLocation(
			r.base, def.pos, def.rot, r.cell, r.world, nullptr, nullptr, RE::ObjectRefHandle(), true, true);
		const auto ref = handle.get();
		if (!ref) {
			logger::warn("Placements: could not place #{} in {:08X}", a_index, r.cell->GetFormID());
			return;
		}
		if (r.owner) {
			ref->SetOwner(r.owner);
		}
		_placed.insert(static_cast<std::uint32_t>(a_index));
		_pendingKeyframe.emplace_back(ref->GetFormID(), 0.0f);
		logger::info("Placed {} ({:08X}) in {}", r.base->GetName(), ref->GetFormID(), r.cell->GetFormEditorID());
	}

	void Manager::OnCellLoaded(RE::TESObjectCELL* a_cell)
	{
		if (!a_cell || !a_cell->IsAttached()) {
			return;  // stale event (e.g. queued just before a save load)
		}
		const auto defs = placements::All();
		for (std::size_t i = 0; i < defs.size(); ++i) {
			if (placements::Get(i).cell == a_cell && !_placed.contains(static_cast<std::uint32_t>(i))) {
				Place(i);
			}
		}
	}

	void Manager::PlaceInAttachedCells()
	{
		const auto defs = placements::All();
		for (std::size_t i = 0; i < defs.size(); ++i) {
			const auto* cell = placements::Get(i).cell;
			if (cell && cell->IsAttached() && !_placed.contains(static_cast<std::uint32_t>(i))) {
				Place(i);
			}
		}
	}

	void Manager::UpdatePendingKeyframes(float a_dt)
	{
		// Like the original's "Don't Havok Settle" placements: books and staves stay exactly where they
		// were put (on shelves, tables) instead of tumbling when their 3D first loads.
		std::erase_if(_pendingKeyframe, [a_dt](auto& entry) {
			entry.second += a_dt;
			auto* ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(entry.first);
			if (!ref || ref->IsDeleted() || entry.second > 15.0f) {
				return true;
			}
			if (!ref->Get3D()) {
				return false;
			}
			ref->SetMotionType(RE::hkpMotion::MotionType::kKeyframed, true);
			return true;
		});
	}

	// ---- Maintenance ------------------------------------------------------------------------------

	void Manager::RecallAll()
	{
		const auto n = _wisps.size();
		for (auto& wisp : _wisps) {
			wisp->Despawn(true);
			Release(*wisp);
		}
		_wisps.clear();
		_claimed.clear();
		logger::info("Recalled {} wisp(s)", n);
	}

	void Manager::ClearCaches()
	{
		harvest::ClearCache();
		_ignoredUntil.clear();
		for (auto& s : _sessions) {
			s.cache.clear();
		}
		logger::info("Cleared learned harvestables and ignore cache");
	}

	void Manager::ResetAll()
	{
		RecallAll();
		ClearCaches();
		_sessions.clear();
		_watched.clear();

		std::size_t orphans = 0;
		if (auto* player = RE::PlayerCharacter::GetSingleton(); player && player->GetParentCell()) {
			const auto& forms = Forms::Get();
			std::vector<RE::TESObjectREFR*> doomed;
			RE::TES::GetSingleton()->ForEachReferenceInRange(player, 1.0e6f, [&](RE::TESObjectREFR* a_ref) {
				auto* base = a_ref ? a_ref->GetBaseObject() : nullptr;
				if (base && (base == forms.wisp || base == forms.wispSilent || base == forms.spawnPoof)) {
					doomed.push_back(a_ref);
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			for (auto* ref : doomed) {
				DeleteTempRef(ref);
			}
			orphans = doomed.size();
			if (forms.harvestStaffAbility && !IsHarvestStaff(player->GetEquippedObject(false)) && !IsHarvestStaff(player->GetEquippedObject(true))) {
				player->RemoveSpell(forms.harvestStaffAbility);
			}
		}
		logger::info("Reset complete ({} orphaned reference(s) removed)", orphans);
	}

	// ---- Save game --------------------------------------------------------------------------------

	void Manager::OnRevert()
	{
		for (auto& wisp : _wisps) {
			wisp->Forget();
		}
		_wisps.clear();
		_sessions.clear();
		_watched.clear();
		_claimed.clear();
		_ignoredUntil.clear();
		_processedTomes.clear();
		_placed.clear();
		_pendingKeyframe.clear();
		_deleteOnNextUpdate.clear();
		_clock = 0.0;
		std::scoped_lock l(_statsLock);
		_stats.totalSent = 0;
		_stats.totalHarvested = 0;
	}

	void Manager::OnPostLoad()
	{
		PlaceInAttachedCells();

		// The staff's harvesting ability should never outlive the staff (e.g. a save made mid-toggle
		// with the staff later removed by another mod).
		const auto& forms = Forms::Get();
		auto*       player = RE::PlayerCharacter::GetSingleton();
		if (player && forms.harvestStaffAbility && !IsHarvestStaff(player->GetEquippedObject(false)) && !IsHarvestStaff(player->GetEquippedObject(true))) {
			player->RemoveSpell(forms.harvestStaffAbility);
		}
	}

	void Manager::SetLoadedRefsToDelete(std::vector<RE::FormID> a_ids)
	{
		_deleteOnNextUpdate = std::move(a_ids);
	}

	void Manager::SetProcessedTomes(std::vector<RE::FormID> a_ids)
	{
		_processedTomes = { a_ids.begin(), a_ids.end() };
	}

	void Manager::SetPlacedIndices(std::vector<std::uint32_t> a_indices)
	{
		_placed = { a_indices.begin(), a_indices.end() };
	}

	std::vector<std::uint32_t> Manager::PlacedIndices() const
	{
		return { _placed.begin(), _placed.end() };
	}

	void Manager::SetTotals(std::uint64_t a_sent, std::uint64_t a_harvested)
	{
		std::scoped_lock l(_statsLock);
		_stats.totalSent = a_sent;
		_stats.totalHarvested = a_harvested;
	}

	std::vector<RE::FormID> Manager::LiveTempRefIDs() const
	{
		std::vector<RE::FormID> ids;
		for (const auto& wisp : _wisps) {
			ids.push_back(wisp->RefID());
		}
		return ids;
	}

	std::vector<RE::FormID> Manager::ProcessedTomes() const
	{
		return { _processedTomes.begin(), _processedTomes.end() };
	}

	Manager::Stats Manager::GetStats() const
	{
		std::scoped_lock l(_statsLock);
		return _stats;
	}
}
