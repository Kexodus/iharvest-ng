#include "Wisp.h"

#include "Forms.h"
#include "Settings.h"

namespace ihng
{
	core::Vec3 ToVec(const RE::NiPoint3& a_p) noexcept { return { a_p.x, a_p.y, a_p.z }; }
	RE::NiPoint3 ToNi(const core::Vec3& a_v) noexcept { return { a_v.x, a_v.y, a_v.z }; }

	void DeleteTempRef(RE::TESObjectREFR* a_ref)
	{
		if (!a_ref || a_ref->IsDeleted()) {
			return;
		}
		a_ref->Disable();
		a_ref->SetDelete(true);
	}

	namespace
	{
		constexpr float kChestHeightFactor = 0.7f;
		constexpr float kMinReturnRadius = 192.0f;  // the original never despawned closer than this

		bool TargetStillThere(RE::TESObjectREFR* a_target, harvest::Kind a_kind)
		{
			if (!a_target || a_target->IsDisabled() || a_target->IsDeleted() || a_target->IsMarkedForDeletion() || !a_target->Is3DLoaded() || !a_target->GetParentCell()) {
				return false;
			}
			if (a_kind == harvest::Kind::kProduce && (a_target->formFlags & RE::TESObjectREFR::RecordFlags::kHarvested)) {
				return false;  // someone (probably the player) picked it in the meantime
			}
			return true;
		}
	}

	std::unique_ptr<Wisp> Wisp::Launch(const LaunchParams& a_params, const Config& a_config)
	{
		const auto& forms = Forms::Get();
		auto*       base = a_config.sounds ? forms.wisp : (forms.wispSilent ? forms.wispSilent : forms.wisp);
		if (!base || !a_params.caster || !a_params.target) {
			return nullptr;
		}

		const RE::NiPoint3 angle{ 0.0f, 0.0f, a_params.casterHeading };
		const auto handle = RE::TESDataHandler::GetSingleton()->CreateReferenceAtLocation(
			base, a_params.spawn, angle, a_params.caster->GetParentCell(), a_params.caster->GetWorldspace(),
			nullptr, nullptr, RE::ObjectRefHandle(), false, true);
		const auto ref = handle.get();
		if (!ref) {
			logger::warn("Wisp: could not create the wisp reference");
			return nullptr;
		}

		std::unique_ptr<Wisp> wisp(new Wisp());
		wisp->_ref = handle;
		wisp->_refID = ref->GetFormID();
		wisp->_caster = a_params.caster->GetHandle();
		wisp->_target = a_params.target->GetHandle();
		wisp->_targetID = a_params.target->GetFormID();
		wisp->_kind = a_params.kind;
		wisp->_lastTargetPoint = harvest::TargetPoint(a_params.target);

		core::FlightTuning tuning;
		tuning.speedMult = a_config.speedMult;
		wisp->_flight.Begin(tuning, ToVec(a_params.spawn), a_params.casterHeading, a_params.spawnBehind,
			wisp->World(a_params.caster, a_params.target, a_config));

		return wisp;
	}

	// Never touches the game: the last wisps can be destroyed during static destruction / DLL unload,
	// when the engine's handle tables may be gone. Every live removal path calls Despawn() or Forget().
	Wisp::~Wisp() = default;

	core::FlightWorld Wisp::World(RE::Actor* a_caster, RE::TESObjectREFR* a_target, const Config& a_config) const
	{
		core::FlightWorld world;
		auto casterPos = a_caster->GetPosition();
		casterPos.z += a_caster->GetHeight() * kChestHeightFactor;
		world.casterPoint = ToVec(casterPos);
		world.returnRadius = std::max(kMinReturnRadius, a_config.returnOffset);
		world.targetValid = !_harvested && TargetStillThere(a_target, _kind);
		world.targetPoint = ToVec(world.targetValid ? harvest::TargetPoint(a_target) : _lastTargetPoint);
		return world;
	}

	void Wisp::MoveRef(RE::TESObjectREFR* a_ref)
	{
		const auto& pos = _flight.Position();
		a_ref->data.location = ToNi(pos);
		a_ref->data.angle = { 0.0f, 0.0f, _flight.Heading() };
		if (a_ref->Get3D()) {
			a_ref->Update3DPosition(true);
		}
	}

	bool Wisp::Update(float a_dt, const Config& a_config)
	{
		if (_despawned) {
			return false;
		}
		const auto ref = _ref.get();
		if (!ref || ref->IsDeleted()) {
			_despawned = true;
			return false;
		}

		const auto caster = _caster.get();
		if (!caster || caster->IsDead() || !caster->GetParentCell() || !ref->GetParentCell() ||
			caster->GetParentCell()->IsInteriorCell() != ref->GetParentCell()->IsInteriorCell() ||
			(caster->GetParentCell()->IsInteriorCell() && caster->GetParentCell() != ref->GetParentCell()) ||
			caster->GetWorldspace() != ref->GetWorldspace() || ref->GetPosition().GetDistance(caster->GetPosition()) > core::kLeashDistance) {
			// Caster gone, loaded through a door, or fast travelled: vanish quietly.
			Despawn(false);
			return false;
		}

		const auto target = _target.get();
		const auto world = World(caster.get(), target.get(), a_config);
		if (world.targetValid) {
			_lastTargetPoint = ToNi(world.targetPoint);
		}

		const auto step = _flight.Update(a_dt, world);
		MoveRef(ref.get());

		if (step.harvestNow) {
			if (target && TargetStillThere(target.get(), _kind)) {
				harvest::Harvest(target.get(), _kind, caster.get());
				_harvested = true;
			}
		}

		if (step.done) {
			if (step.timedOut) {
				logger::warn("Wisp {:08X} hit its lifetime cap and was force-despawned (target {:08X}, harvested={})", _refID, _targetID, _harvested);
			}
			Despawn(true);
			return false;
		}
		return true;
	}

	void Wisp::Forget() noexcept
	{
		// The world is being torn down; nothing of ours may be touched.
		_despawned = true;
	}

	void Wisp::Despawn(bool a_withEffects)
	{
		if (_despawned) {
			return;
		}
		_despawned = true;

		const auto ref = _ref.get();
		if (!ref) {
			return;
		}
		if (a_withEffects && ref->GetParentCell()) {
			const auto config = Settings::GetSingleton().Get();
			const auto pos = ref->GetPosition();
			if (config.sounds) {
				fx::PlayDespawnSound(pos);
			}
		}
		DeleteTempRef(ref.get());
	}

	namespace fx
	{
		void PlayDespawnSound(const RE::NiPoint3& a_pos)
		{
			auto* sound = Forms::Get().despawnSound;
			auto* audio = RE::BSAudioManager::GetSingleton();
			if (!sound || !sound->descriptor || !audio) {
				return;
			}
			RE::BSSoundHandle handle;
			if (audio->GetSoundHandle(handle, sound->descriptor)) {
				handle.SetPosition(a_pos);
				handle.Play();
			}
		}
	}
}
