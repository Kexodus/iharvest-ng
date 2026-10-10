#include "Harvestable.h"

namespace ihng::harvest
{
	namespace
	{
		using ObjectPtr = RE::BSTSmartPointer<RE::BSScript::Object>;

		// base FormID -> learned kind. kNone entries are "examined but not harvestable".
		std::unordered_map<RE::FormID, Kind> g_learned;

		bool IsA(const RE::BSScript::ObjectTypeInfo* a_type, const char* a_className)
		{
			for (auto* t = a_type; t; t = t->GetParent()) {
				if (_stricmp(t->GetName(), a_className) == 0) {
					return true;
				}
			}
			return false;
		}

		// The script instance bound to a_ref that is (or derives from) a_className - the native
		// equivalent of Papyrus `ref as ClassName`. Vanilla critters use subclasses (CritterMoth,
		// CritterFish...) of Critter, so inheritance matters.
		ObjectPtr FindScript(RE::TESObjectREFR* a_ref, const char* a_className)
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm || !a_ref) {
				return {};
			}
			auto* policy = vm->GetObjectHandlePolicy();
			if (!policy) {
				return {};
			}
			const auto handle = policy->GetHandleForObject(a_ref->GetFormType(), a_ref);
			if (handle == policy->EmptyHandle()) {
				return {};
			}
			RE::BSSpinLockGuard locker(vm->attachedScriptsLock);
			const auto it = vm->attachedScripts.find(handle);
			if (it == vm->attachedScripts.end()) {
				return {};
			}
			for (auto& attached : it->second) {
				auto* object = attached.get();
				if (object && IsA(object->GetTypeInfo(), a_className)) {
					return ObjectPtr(object);
				}
			}
			return {};
		}

		// Auto-property value lookup that also accounts for variables of ancestor classes (the
		// object's variable block is laid out base class first).
		const RE::BSScript::Variable* Prop(const ObjectPtr& a_obj, const char* a_name)
		{
			constexpr auto        kInvalid = static_cast<std::uint32_t>(-1);
			const RE::BSFixedString name(a_name);
			for (auto* cls = a_obj->GetTypeInfo(); cls; cls = cls->GetParent()) {
				const auto idx = cls->GetPropertyIndex(name);
				if (idx == kInvalid) {
					continue;
				}
				std::uint32_t offset = 0;
				for (auto* p = cls->GetParent(); p; p = p->GetParent()) {
					offset += p->GetNumVariables();
				}
				return std::addressof(a_obj->variables[offset + idx]);
			}
			return nullptr;
		}

		std::int32_t IntProp(const ObjectPtr& a_obj, const char* a_name)
		{
			const auto* var = Prop(a_obj, a_name);
			return var && var->IsInt() ? var->GetSInt() : 0;
		}

		RE::TESForm* FormProp(const ObjectPtr& a_obj, const char* a_name)
		{
			const auto* var = Prop(a_obj, a_name);
			if (!var || !var->IsObject()) {
				return nullptr;
			}
			return var->Unpack<RE::TESForm*>();
		}

		bool IsFromHearthfire(const RE::TESForm* a_form)
		{
			const auto* file = a_form ? a_form->GetFile(0) : nullptr;
			return file && _stricmp(file->GetFilename().data(), "HearthFires.esm") == 0;
		}

		// IH_Util.ProducesIngredient(): can this form (ingredient / leveled list / form list) yield
		// something we harvest? Recursion is bounded (circular lists exist in some load orders).
		bool Produces(RE::TESForm* a_form, const Rules& a_rules, int a_depth = 0)
		{
			if (!a_form || a_depth > 16) {
				return false;
			}
			switch (a_form->GetFormType()) {
			case RE::FormType::Ingredient:
				return true;
			case RE::FormType::AlchemyItem:
				return a_rules.food && static_cast<RE::AlchemyItem*>(a_form)->IsFood();
			case RE::FormType::Misc:
				// Hearthfire planters yield hidden "BYOHHouseIngredient*" misc items whose script turns
				// them into the real ingredient when they land in an inventory. Because NG harvests as
				// the caster, that conversion simply happens - no vanilla script patches needed.
				return a_rules.hearthfire && IsFromHearthfire(a_form);
			case RE::FormType::LeveledItem:
				{
					auto* list = static_cast<RE::TESLevItem*>(a_form);
					if (list->GetChanceNone() >= 100) {
						return false;
					}
					for (auto& entry : list->entries) {
						if (entry.count > 0 && Produces(entry.form, a_rules, a_depth + 1)) {
							return true;
						}
					}
					return false;
				}
			case RE::FormType::FormList:
				{
					bool found = false;
					static_cast<RE::BGSListForm*>(a_form)->ForEachForm([&](RE::TESForm* f) {
						if (Produces(f, a_rules, a_depth + 1)) {
							found = true;
							return RE::BSContainer::ForEachResult::kStop;
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});
					return found;
				}
			default:
				return false;
			}
		}

		// IH_FloraLearnerScript.ShouldLearnActivator(): scripted harvestables, detected by the script
		// bound to the reference and its properties (read straight from the Papyrus VM, no Papyrus run).
		bool IsHarvestableActivator(RE::TESObjectREFR* a_ref, const Rules& a_rules)
		{
			if (auto s = FindScript(a_ref, "Critter")) {
				return IntProp(s, "lootableCount") > 0 &&
				       (FormProp(s, "lootable") || Produces(FormProp(s, "nonIngredientLootable"), a_rules));
			}
			if (auto s = FindScript(a_ref, "FXfakeCritterScript")) {
				return (IntProp(s, "numberOfIngredientsOnCatch") > 0 && FormProp(s, "myIngredient")) ||
				       (a_rules.food && FormProp(s, "myFood"));
			}
			if (FindScript(a_ref, "NirnrootACTIVATORScript") || FindScript(a_ref, "USKP_NirnrootACTIVATORScript")) {
				return true;
			}
			if (auto s = FindScript(a_ref, "DLC1TrapPoisonBloom")) {
				return FormProp(s, "myIngredient") || Produces(FormProp(s, "myMiscObject"), a_rules) ||
				       Produces(FormProp(s, "myPotion"), a_rules);
			}
			if (auto s = FindScript(a_ref, "ccBGSSSE025_HarvestableActivator")) {
				auto* rare = FormProp(s, "leveledRareCuriosItem");
				return FormProp(s, "itemToHarvest") || (rare && Produces(rare, a_rules));
			}
			// Wisp cores (vanilla + Harvest Overhaul Redone) and Beyond Skyrim: Bruma wisp stalks.
			return FindScript(a_ref, "wispCoreScript") || FindScript(a_ref, "wispCoreScript_lyu") ||
			       FindScript(a_ref, "BSKWispStalkACTIVATORScript");
		}

		Kind ClassifyBase(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_base, const Rules& a_rules)
		{
			switch (a_base->GetFormType()) {
			case RE::FormType::Ingredient:
				return Kind::kLooseItem;
			case RE::FormType::AlchemyItem:
				return a_rules.food && static_cast<RE::AlchemyItem*>(a_base)->IsFood() ? Kind::kLooseItem : Kind::kNone;
			case RE::FormType::Ammo:
				return a_rules.arrows && a_base->GetPlayable() ? Kind::kLooseItem : Kind::kNone;
			case RE::FormType::Tree:
			case RE::FormType::Flora:
				if (auto* produce = a_base->As<RE::TESProduceForm>(); produce && Produces(produce->produceItem, a_rules)) {
					return Kind::kProduce;
				}
				return Kind::kNone;
			case RE::FormType::Activator:
				return a_rules.critters && IsHarvestableActivator(a_ref, a_rules) ? Kind::kScripted : Kind::kNone;
			default:
				return Kind::kNone;
			}
		}

		bool IsArrowRecoverable(RE::TESObjectREFR* a_ref)
		{
			auto* proj = a_ref->As<RE::ArrowProjectile>();
			if (!proj) {
				return false;
			}
			auto& data = proj->GetProjectileRuntimeData();
			if (!data.ammoSource || !data.ammoSource->GetPlayable() || data.flags.any(RE::Projectile::Flags::kDestroyed)) {
				return false;
			}
			// Only arrows that have come to rest (stuck in the world), never ones still in flight.
			return !data.impacts.empty() && data.linearVelocity.Length() < 1.0f;
		}

		// IH_Util.AllowedToTake(): owner NPC must be the taker; owner faction must contain the taker.
		bool AllowedToTake(RE::TESObjectREFR* a_ref, RE::Actor* a_taker)
		{
			auto* owner = a_ref->GetOwner();
			if (!owner) {
				return true;
			}
			if (auto* npc = owner->As<RE::TESNPC>()) {
				return a_taker->GetActorBase() == npc;
			}
			if (auto* faction = owner->As<RE::TESFaction>()) {
				return a_taker->IsInFaction(faction);
			}
			return true;
		}

		bool IsHarvestedState(RE::TESObjectREFR* a_ref)
		{
			// Scripted harvestables flip into a "harvested" state (e.g. Creation Club harvestables).
			for (const char* name : { "ccBGSSSE025_HarvestableActivator", "NirnrootACTIVATORScript", "USKP_NirnrootACTIVATORScript" }) {
				if (auto s = FindScript(a_ref, name); s && _stricmp(s->currentState.c_str(), "harvested") == 0) {
					return true;
				}
			}
			return false;
		}
	}

	Kind Classify(RE::TESObjectREFR* a_ref, const Rules& a_rules)
	{
		if (!a_ref) {
			return Kind::kNone;
		}
		if (a_ref->GetFormType() == RE::FormType::ProjectileArrow) {
			return a_rules.arrows ? Kind::kArrow : Kind::kNone;
		}
		auto* base = a_ref->GetBaseObject();
		if (!base) {
			return Kind::kNone;
		}
		if (const auto it = g_learned.find(base->GetFormID()); it != g_learned.end()) {
			return it->second;
		}
		const Kind kind = ClassifyBase(a_ref, base, a_rules);
		g_learned.emplace(base->GetFormID(), kind);
		if (kind != Kind::kNone) {
			logger::debug("Learned {:08X} ({}) as harvestable kind {}", base->GetFormID(), base->GetName(), std::to_underlying(kind));
		}
		return kind;
	}

	void ClearCache()
	{
		g_learned.clear();
	}

	std::size_t LearnedCount()
	{
		return static_cast<std::size_t>(std::ranges::count_if(g_learned, [](const auto& kv) { return kv.second != Kind::kNone; }));
	}

	std::size_t ExaminedCount()
	{
		return g_learned.size();
	}

	bool IsAvailable(RE::TESObjectREFR* a_ref, Kind a_kind, RE::Actor* a_caster, bool a_respectOwnership)
	{
		if (!a_ref || a_kind == Kind::kNone || a_ref->IsDisabled() || a_ref->IsDeleted() || a_ref->IsMarkedForDeletion() || !a_ref->Is3DLoaded()) {
			return false;
		}
		switch (a_kind) {
		case Kind::kProduce:
			if (a_ref->formFlags & RE::TESObjectREFR::RecordFlags::kHarvested) {
				return false;
			}
			break;
		case Kind::kScripted:
			if (IsHarvestedState(a_ref)) {
				return false;
			}
			break;
		case Kind::kArrow:
			if (!IsArrowRecoverable(a_ref)) {
				return false;
			}
			break;
		default:
			break;
		}
		// Activation-blocked activators are still fine (the original exempted them: blocking does
		// nothing for activators, and it would skip some vanilla poison blooms).
		if (a_kind != Kind::kScripted && a_ref->IsActivationBlocked()) {
			return false;
		}
		if (a_respectOwnership && a_kind != Kind::kArrow) {
			// For the player, the engine's own crime test is the authority (it decides "Take" vs "Steal"
			// and knows ownership granted by quests/mods, e.g. being made steward of the Blue Palace). The
			// original's NPC-or-faction test is only a fallback for NPC casters.
			if (a_caster == RE::PlayerCharacter::GetSingleton()) {
				if (a_ref->IsCrimeToActivate()) {
					return false;
				}
			} else if (!AllowedToTake(a_ref, a_caster)) {
				return false;
			}
		}
		return true;
	}

	bool Harvest(RE::TESObjectREFR* a_ref, Kind a_kind, RE::Actor* a_caster)
	{
		if (!a_ref || !a_caster || a_kind == Kind::kNone) {
			return false;
		}
		// Activation by the caster is exactly what happens when they press Activate on it themselves:
		// flora yields (Green Thumb and all), loose items/arrows are picked up, critter scripts fire.
		// Same arguments as Papyrus ObjectReference.Activate(akActivator): a whole stack is picked up at once.
		return a_ref->ActivateRef(a_caster, 0, nullptr, 1, false);
	}

	RE::NiPoint3 TargetPoint(RE::TESObjectREFR* a_ref)
	{
		if (auto* root = a_ref->Get3D()) {
			const auto& bound = root->worldBound;
			if (bound.radius > 0.0f) {
				return bound.center;
			}
			return root->world.translate;
		}
		auto p = a_ref->GetPosition();
		p.z += 0.5f * a_ref->GetHeight();
		return p;
	}
}
