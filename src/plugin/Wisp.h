#pragma once

#include "Harvestable.h"

#include <ihng/core/Flight.h>

namespace ihng
{
	struct Config;

	// One flying wisp: a temporary, collision-less activator reference (IHNG_Wisp) moved natively every
	// frame along a core::Flight path. Replaces the original's pooled Getter Critter *actors*, their AI
	// travel packages, 64 pathing markers and ~700 lines of latent Papyrus per trip.
	class Wisp
	{
	public:
		struct LaunchParams
		{
			RE::Actor*          caster{ nullptr };
			RE::TESObjectREFR*  target{ nullptr };
			harvest::Kind       kind{ harvest::Kind::kNone };
			RE::NiPoint3        spawn;
			float               casterHeading{ 0.0f };
			bool                spawnBehind{ false };
		};

		// Places the wisp reference and starts the flight. Returns nullptr if the reference could not be created.
		[[nodiscard]] static std::unique_ptr<Wisp> Launch(const LaunchParams& a_params, const Config& a_config);

		~Wisp();
		Wisp(const Wisp&) = delete;
		Wisp& operator=(const Wisp&) = delete;

		// Advances one frame. Returns false once the wisp has finished and despawned.
		bool Update(float a_dt, const Config& a_config);

		// Ends the flight right now (recall / caster gone).
		void Despawn(bool a_withEffects);

		// Drops the wisp without touching the game world (the world is being torn down: new game / load).
		void Forget() noexcept;

		[[nodiscard]] RE::FormID       RefID() const noexcept { return _refID; }
		[[nodiscard]] RE::FormID       TargetID() const noexcept { return _targetID; }
		[[nodiscard]] RE::ActorHandle  Caster() const noexcept { return _caster; }
		[[nodiscard]] bool             Harvested() const noexcept { return _harvested; }

	private:
		Wisp() = default;

		void MoveRef(RE::TESObjectREFR* a_ref);
		[[nodiscard]] core::FlightWorld World(RE::Actor* a_caster, RE::TESObjectREFR* a_target, const Config& a_config) const;

		RE::ObjectRefHandle _ref;
		RE::FormID          _refID{ 0 };
		RE::ActorHandle     _caster;
		RE::ObjectRefHandle _target;
		RE::FormID          _targetID{ 0 };
		harvest::Kind       _kind{ harvest::Kind::kNone };
		core::Flight        _flight;
		RE::NiPoint3        _lastTargetPoint;
		bool                _harvested{ false };
		bool                _despawned{ false };

	};

	namespace fx
	{
		void PlayDespawnSound(const RE::NiPoint3& a_pos);
	}

	[[nodiscard]] core::Vec3   ToVec(const RE::NiPoint3& a_p) noexcept;
	[[nodiscard]] RE::NiPoint3 ToNi(const core::Vec3& a_v) noexcept;

	// Disables and deletes a temporary reference we created (wisps; poofs from older builds).
	void DeleteTempRef(RE::TESObjectREFR* a_ref);
}
