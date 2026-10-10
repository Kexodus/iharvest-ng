#include "ihng/core/CastTiming.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace ihng::core;
using Catch::Approx;

TEST_CASE("Spell cast parameters match the original IHarvest script math", "[timing]")
{
	SECTION("no skills")
	{
		const auto p = ComputeCastParams(kSpellTuning, 0, 0, false);
		CHECK(p.delayUs == 1'000'000);
		CHECK(p.minDelayUs == 500'000);
		CHECK(p.accel == Approx(0.85f));
		CHECK(p.radius == Approx(1500.0f));
	}
	SECTION("100 Alteration / 100 Alchemy")
	{
		const auto p = ComputeCastParams(kSpellTuning, 100, 100, false);
		CHECK(p.minDelayUs == 500'000 - 111'100 - 55'500);
		CHECK(p.accel == Approx(0.85f - 0.15f));
		CHECK(p.radius == Approx(1500.0f + 1500.0f + 1000.0f));
	}
	SECTION("dual casting widens the radius and speeds up the ramp")
	{
		const auto p = ComputeCastParams(kSpellTuning, 50, 20, true);
		const std::int32_t radius = 1500 + 15 * 50 + 10 * 20;
		CHECK(p.radius == Approx(static_cast<float>(radius * 150 / 100)));
		CHECK(p.minDelayUs == (500'000 - 1111 * 50 - 555 * 20) * 75 / 100);
		CHECK(p.accel == Approx((0.85f - (1000 * 50 + 500 * 20) / 1e6f) * 0.85f));
	}
	SECTION("absurd skills never drop below the absolute floor")
	{
		const auto p = ComputeCastParams(kSpellTuning, 1000, 1000, true);
		CHECK(p.minDelayUs == kAbsoluteMinDelayUs);
		CHECK(p.accel >= 0.05f);
	}
}

TEST_CASE("Staff parameters use the staff tuning", "[timing]")
{
	const auto p = ComputeCastParams(kStaffTuning, 100, 100, false);
	CHECK(p.delayUs == 1'500'000);
	CHECK(p.minDelayUs == 300'000 - 66'700 - 33'300);
	CHECK(p.radius == Approx(3000.0f + 3300.0f + 1700.0f));
}

TEST_CASE("Wisp cost honours Alteration cost reduction with the original floor", "[timing]")
{
	CHECK(ComputeWispCost(kSpellTuning, 8.0f, 0.0f) == Approx(8.0f));
	CHECK(ComputeWispCost(kSpellTuning, 8.0f, 50.0f) == Approx(4.0f));
	CHECK(ComputeWispCost(kSpellTuning, 8.0f, 100.0f) == Approx(8.0f * 0.15f));  // MinSpellReduction
	CHECK(ComputeWispCost(kStaffTuning, 12.0f, 100.0f) == Approx(0.0f));         // staff has no floor
	CHECK(ComputeWispCost(kSpellTuning, 8.0f, -25.0f) == Approx(10.0f));
}

TEST_CASE("SpawnClock fires immediately, then after maxDelay, then accelerates to minDelay", "[timing]")
{
	const auto  params = ComputeCastParams(kSpellTuning, 0, 0, false);
	SpawnClock clock(params);

	REQUIRE(clock.Tick(0.0));  // first wisp at cast start
	clock.Report(AttemptResult::kDispatched);
	CHECK(clock.SecondsUntilNext() == Approx(1.0));

	CHECK_FALSE(clock.Tick(0.99));
	REQUIRE(clock.Tick(0.02));  // second wisp at ~1.0 s
	CHECK(clock.CurrentDelayUs() == 850'000);
	CHECK(clock.SecondsUntilNext() == Approx(0.85));

	// Keep firing; the delay must converge on and stay at minDelay.
	for (int i = 0; i < 50; ++i) {
		while (!clock.Tick(0.01)) {}
		clock.Report(AttemptResult::kDispatched);
	}
	CHECK(clock.CurrentDelayUs() == params.minDelayUs);
}

TEST_CASE("SpawnClock backs off with slowMode^0.66 when nothing is found", "[timing]")
{
	SpawnClock clock(ComputeCastParams(kSpellTuning, 0, 0, false));
	REQUIRE(clock.Tick(0.0));
	clock.Report(AttemptResult::kNothingFound);  // slowMode 1
	clock.Report(AttemptResult::kAtCap);         // slowMode 4
	CHECK(clock.SlowMode() == 4);
	REQUIRE(clock.Tick(1.0));
	CHECK(clock.SecondsUntilNext() == Approx(0.85 * std::pow(4.0, 0.66)).epsilon(1e-4));
	clock.Report(AttemptResult::kDispatched);
	CHECK(clock.SlowMode() == 0);
}
