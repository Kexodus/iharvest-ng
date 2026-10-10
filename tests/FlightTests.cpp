#include "ihng/core/Flight.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace ihng::core;
using Catch::Approx;

namespace
{
	struct Result
	{
		bool  harvested{ false };
		bool  done{ false };
		bool  timedOut{ false };
		float harvestTime{ -1.0f };
		float doneTime{ -1.0f };
		float closestToTarget{ 1e9f };
	};

	Result Fly(Flight& a_flight, FlightWorld a_world, float a_dt = 1.0f / 60.0f, float a_maxTime = 40.0f, Vec3 a_casterVelocity = {})
	{
		Result r;
		for (float t = 0.0f; t < a_maxTime; t += a_dt) {
			a_world.casterPoint += a_casterVelocity * a_dt;
			const auto step = a_flight.Update(a_dt, a_world);
			r.closestToTarget = std::min(r.closestToTarget, Distance(a_flight.Position(), a_world.targetPoint));
			if (step.harvestNow) {
				REQUIRE_FALSE(r.harvested);  // exactly once
				r.harvested = true;
				r.harvestTime = t;
			}
			if (step.done) {
				r.done = true;
				r.timedOut = step.timedOut;
				r.doneTime = t;
				break;
			}
		}
		return r;
	}
}

TEST_CASE("A wisp flies out, harvests exactly once, and comes back", "[flight]")
{
	FlightWorld world{ .casterPoint = { 0, 0, 100 }, .targetPoint = { 0, 1200, 20 }, .targetValid = true, .returnRadius = 192 };
	Flight      f;
	f.Begin(FlightTuning{}, { 0, 192, 40 }, 0.0f, false, world);
	CHECK(f.Phase() == FlightPhase::kOutbound);

	const auto r = Fly(f, world);
	CHECK(r.harvested);
	CHECK(r.done);
	CHECK_FALSE(r.timedOut);
	CHECK(r.closestToTarget <= FlightTuning{}.pickContact + 0.01f);
	CHECK(Distance(f.Position(), world.casterPoint) <= 192.0f + 1.0f);
	// ~1000 units each way at ~1000 u/s plus the slow dive: a few seconds, not tens.
	CHECK(r.doneTime < 6.0f);
}

TEST_CASE("Losing the target before contact aborts the harvest and returns", "[flight]")
{
	FlightWorld world{ .casterPoint = { 0, 0, 100 }, .targetPoint = { 800, 800, 0 }, .targetValid = true };
	Flight      f;
	f.Begin(FlightTuning{}, { 0, 192, 40 }, 0.0f, false, world);
	for (int i = 0; i < 10; ++i) {
		f.Update(1.0f / 60.0f, world);
	}
	world.targetValid = false;
	const auto r = Fly(f, world);
	CHECK_FALSE(r.harvested);
	CHECK(r.done);
}

TEST_CASE("Return tracks a moving caster", "[flight]")
{
	FlightWorld world{ .casterPoint = { 0, 0, 100 }, .targetPoint = { -600, 600, 0 }, .targetValid = true };
	Flight      f;
	f.Begin(FlightTuning{}, { 0, 192, 40 }, 0.0f, false, world);
	const Vec3 runVelocity{ 0, 350, 0 };  // running away from the spawn point
	const auto r = Fly(f, world, 1.0f / 60.0f, 40.0f, runVelocity);
	CHECK(r.harvested);
	CHECK(r.done);
	CHECK_FALSE(r.timedOut);
}

TEST_CASE("A target right next to the spawn goes straight to the dive", "[flight]")
{
	FlightWorld world{ .casterPoint = { 0, 0, 100 }, .targetPoint = { 0, 230, 30 }, .targetValid = true };
	Flight      f;
	f.Begin(FlightTuning{}, { 0, 192, 40 }, 0.0f, false, world);
	CHECK(f.Phase() == FlightPhase::kPick);
	CHECK(Fly(f, world).harvested);
}

TEST_CASE("Flights always end, even with an unreachable target", "[flight]")
{
	FlightTuning t;
	t.maxLifetime = 3.0f;
	t.speedMult = 0.1f;
	FlightWorld world{ .casterPoint = { 0, 0, 100 }, .targetPoint = { 0, 100000, 0 }, .targetValid = true };
	Flight      f;
	f.Begin(t, { 0, 192, 40 }, 0.0f, false, world);
	const auto r = Fly(f, world);
	CHECK(r.done);
	CHECK(r.timedOut);
}

TEST_CASE("Launch never heads straight back through the caster", "[flight]")
{
	// Target directly behind the caster (caster faces +Y, target at -Y).
	FlightWorld world{ .casterPoint = { 0, 0, 100 }, .targetPoint = { 0, -1500, 0 }, .targetValid = true };
	Flight      f;
	f.Begin(FlightTuning{}, { 0, 192, 40 }, 0.0f, false, world);
	const float launch = f.Heading();
	CHECK(std::fabs(WrapAngle(launch)) <= FlightTuning{}.maxLaunchOffset + 1e-4f);
	CHECK(Fly(f, world).harvested);
}

TEST_CASE("WrapAngle and heading helpers", "[flight]")
{
	CHECK(WrapAngle(0.0f) == Approx(0.0f));
	CHECK(WrapAngle(7.0f) == Approx(7.0f - 6.2831853f));
	CHECK(DirToHeading(HeadingToDir(1.0f)) == Approx(1.0f));
	CHECK(HeadingToDir(0.0f).y == Approx(1.0f));
}

TEST_CASE("At 20x speed a wisp still harvests exactly once and returns", "[flight]")
{
	FlightTuning t;
	t.speedMult = 20.0f;
	for (const float dt : { 1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 144.0f }) {
		FlightWorld world{ .casterPoint = { 0, 0, 100 }, .targetPoint = { 900, 2400, 15 }, .targetValid = true, .returnRadius = 192 };
		Flight      f;
		f.Begin(t, { 0, 192, 40 }, 0.0f, false, world);
		const auto r = Fly(f, world, dt);
		CHECK(r.harvested);
		CHECK(r.done);
		CHECK_FALSE(r.timedOut);
		CHECK(r.closestToTarget <= t.pickContact + 0.01f);
		CHECK(r.doneTime < 1.0f);  // ~5000 units round trip at 20000 u/s, plus a sped-up dive
	}
}
