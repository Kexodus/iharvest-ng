#pragma once

#include <cstdint>

namespace ihng::core
{
	// Tuning of the "refire" behaviour of a harvest cast. The numbers are the exact property values the
	// original IHarvest shipped on IH_HarvestEffect (spell) and IH_HarvestStaffEffect (staff), so the
	// cast ramps up, caps and scales with skills identically. Times are microseconds, like the original.
	struct CastTuning
	{
		std::int32_t maxDelayUs;              // delay before the 2nd wisp
		std::int32_t minDelayBaseUs;          // fastest refire with no skills
		std::int32_t minDelayPerAlteration;   // minDelay reduction per Alteration point
		std::int32_t minDelayPerAlchemy;      // minDelay reduction per Alchemy point
		std::int32_t accelPerAlteration;      // accel bonus (x1e-6) per Alteration point
		std::int32_t accelPerAlchemy;         // accel bonus (x1e-6) per Alchemy point
		std::int32_t dualCastAccelPct;        // accel multiplier when dual casting (percent)
		std::int32_t dualCastMinDelayPct;     // minDelay multiplier when dual casting (percent)
		std::int32_t maxRadiusBase;           // search radius with no skills
		std::int32_t radiusPerAlteration;
		std::int32_t radiusPerAlchemy;
		std::int32_t dualCastRadiusPct;       // radius multiplier when dual casting (percent)
		float        minCostFraction;         // cost never drops below this fraction of base (Alteration cost reduction)
	};

	inline constexpr CastTuning kSpellTuning{ 1'000'000, 500'000, 1111, 555, 1000, 500, 85, 75, 1500, 15, 10, 150, 0.15f };
	inline constexpr CastTuning kStaffTuning{ 1'500'000, 300'000, 667, 333, 800, 400, 100, 100, 3000, 33, 17, 100, 0.0f };

	// Never refire faster than this, whatever the skill level (guards against modded skills > 100).
	inline constexpr std::int32_t kAbsoluteMinDelayUs = 50'000;

	struct CastParams
	{
		std::int32_t delayUs;     // current delay (starts at maxDelay)
		std::int32_t minDelayUs;  // floor the delay accelerates down to
		float        accel;       // per-refire multiplier on delay (< 1)
		float        radius;      // search radius in world units
	};

	[[nodiscard]] CastParams ComputeCastParams(const CastTuning& a_tuning, std::int32_t a_alteration, std::int32_t a_alchemy, bool a_dualCast) noexcept;

	// Per-wisp cost after Alteration cost reduction (the "AlterationMod" actor value, in percent).
	[[nodiscard]] float ComputeWispCost(const CastTuning& a_tuning, float a_baseCost, float a_alterationModPct) noexcept;

	enum class AttemptResult : std::uint8_t
	{
		kDispatched,     // a wisp was sent
		kNothingFound,   // nothing harvestable in range -> back off (slowMode += 1)
		kAtCap,          // concurrent wisp cap reached  -> back off harder (slowMode += 3)
		kNeutral,        // e.g. busy/aborted; timing unchanged
	};

	// Drives when a harvest cast should attempt to dispatch the next wisp. Mirrors the original
	// script exactly: an attempt fires immediately when the cast starts, the next after maxDelay, and
	// each following interval is delay *= accel (floored at minDelay), stretched by slowMode^0.66 when
	// recent attempts found nothing or hit the wisp cap.
	class SpawnClock
	{
	public:
		explicit SpawnClock(const CastParams& a_params) noexcept;

		// Advances time by a_dt seconds. Returns true when an attempt is due (at most once per call).
		[[nodiscard]] bool Tick(double a_dt) noexcept;

		void Report(AttemptResult a_result) noexcept;

		[[nodiscard]] std::int32_t CurrentDelayUs() const noexcept { return _delayUs; }
		[[nodiscard]] std::int32_t SlowMode() const noexcept { return _slowMode; }
		[[nodiscard]] double       SecondsUntilNext() const noexcept { return _untilNext; }

	private:
		std::int32_t _delayUs;
		std::int32_t _minDelayUs;
		float        _accel;
		std::int32_t _slowMode{ 0 };
		double       _untilNext{ 0.0 };
		bool         _started{ false };
	};
}
