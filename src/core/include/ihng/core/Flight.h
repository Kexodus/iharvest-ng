#pragma once

#include "ihng/core/Vec3.h"

#include <cstdint>

namespace ihng::core
{
	// A wisp further than this from its caster vanishes (caster ran off / teleported).
	inline constexpr float kLeashDistance = 12000.0f;
	// Searches never look further than a wisp can fly without hitting the leash (margin for the arc).
	inline constexpr float kMaxSearchRadius = kLeashDistance - 2000.0f;

	// Wisp flight tuning. Speeds are world units/second. Defaults mirror the original Getter Critter:
	// 1000 u/s movement type, a 500 u/s launch burst 320 units out and 64 up, a stop 96 units short of
	// the target followed by a slow 250 u/s "picking" dive, and a return to ~192 units from the caster.
	struct FlightTuning
	{
		float cruiseSpeed{ 1000.0f };
		float launchSpeed{ 500.0f };
		float accelTime{ 0.35f };         // seconds to ramp from launchSpeed to cruiseSpeed
		float launchDistance{ 320.0f };
		float launchRise{ 64.0f };
		float approachRadius{ 96.0f };
		float pickSpeed{ 250.0f };
		float pickContact{ 10.0f };
		float pickMaxTime{ 1.5f };
		float backoffSpeed{ 700.0f };
		float backoffMaxTime{ 0.6f };
		float arcFactor{ 0.15f };         // arc height as a fraction of the leg's length...
		float minArc{ 32.0f };            // ...clamped to [minArc, maxArc]
		float maxArc{ 256.0f };
		float maxLifetime{ 25.0f };       // hard cap; the wisp despawns wherever it is (raised per
		                                  // flight for long or slow trips, see Flight::Begin)
		float maxLaunchOffset{ 2.44346f };  // 140 deg: never launch straight back into the caster's face
		float minLaunchOffsetBehind{ 0.69813f };  // 40 deg: when spawning behind the caster
		float speedMult{ 1.0f };          // user multiplier on all travel speeds
	};

	enum class FlightPhase : std::uint8_t
	{
		kOutbound,  // spawn -> just short of the target, on an arc
		kPick,      // slow dive into the target
		kBackoff,   // pop back out to where the dive started
		kReturn,    // arc back to the caster (tracks a moving caster)
		kDone,
	};

	// Per-frame world state the flight reacts to.
	struct FlightWorld
	{
		Vec3  casterPoint;            // roughly chest height of the caster
		Vec3  targetPoint;            // the part of the target to fly into
		bool  targetValid{ true };    // false once the target is gone (picked up by someone, disabled, unloaded)
		float returnRadius{ 192.0f }; // how close to the caster the wisp returns before despawning
	};

	struct FlightStep
	{
		bool harvestNow{ false };  // the wisp just touched the target: harvest it this frame
		bool done{ false };        // flight finished (or timed out): despawn
		bool timedOut{ false };
	};

	class Flight
	{
	public:
		// a_spawnBehind: the spawn point is behind the caster (negative spawn distance multiplier).
		void Begin(const FlightTuning& a_tuning, const Vec3& a_spawn, float a_casterHeading, bool a_spawnBehind, const FlightWorld& a_world) noexcept;

		FlightStep Update(float a_dt, const FlightWorld& a_world) noexcept;

		[[nodiscard]] const Vec3& Position() const noexcept { return _pos; }
		[[nodiscard]] float       Heading() const noexcept { return _heading; }
		[[nodiscard]] FlightPhase Phase() const noexcept { return _phase; }
		[[nodiscard]] float       Age() const noexcept { return _age; }
		[[nodiscard]] bool        Harvested() const noexcept { return _harvested; }

	private:
		void EnterReturn(const FlightWorld& a_world) noexcept;
		void MoveTowards(const Vec3& a_goal, float a_distance) noexcept;
		[[nodiscard]] float Arc(float a_legLength) const noexcept;
		[[nodiscard]] Vec3  ApproachPoint(const Vec3& a_target) const noexcept;
		[[nodiscard]] Vec3  ReturnPoint(const FlightWorld& a_world) const noexcept;
		[[nodiscard]] float CurrentSpeed() const noexcept;

		FlightTuning _t{};
		FlightPhase  _phase{ FlightPhase::kDone };
		Vec3         _pos{};
		Vec3         _segStart{};
		Vec3         _segControl{};
		Vec3         _pickStart{};
		float        _s{ 0.0f };        // progress along the current Bezier leg, 0..1
		float        _heading{ 0.0f };
		float        _age{ 0.0f };
		float        _phaseAge{ 0.0f };
		bool         _harvested{ false };
	};
}
