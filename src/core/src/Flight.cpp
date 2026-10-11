#include "ihng/core/Flight.h"

#include <algorithm>
#include <cmath>

namespace ihng::core
{
	namespace
	{
		constexpr Vec3 kUp{ 0.0f, 0.0f, 1.0f };
	}

	void Flight::Begin(const FlightTuning& a_tuning, const Vec3& a_spawn, float a_casterHeading, bool a_spawnBehind, const FlightWorld& a_world) noexcept
	{
		_t = a_tuning;
		_t.speedMult = std::clamp(_t.speedMult, 0.1f, 25.0f);
		_pos = a_spawn;
		_segStart = a_spawn;
		_s = 0.0f;
		_age = 0.0f;
		_phaseAge = 0.0f;
		_harvested = false;

		const Vec3  toTarget = a_world.targetPoint - a_spawn;
		const float dist = toTarget.Length();

		// The cap must outlast the round trip: a far target at a low speed multiplier can need far more
		// than 25 s, and timing out there costs magicka for nothing, every refire. Bounded by the leash
		// (nothing further is ever reachable), so every flight still ends.
		const float trip = std::min(dist, kLeashDistance);
		_t.maxLifetime = std::max(_t.maxLifetime, 10.0f + 3.0f * trip / (_t.cruiseSpeed * _t.speedMult));

		// Launch direction: towards the target, but never more than 140 deg off the caster's facing (or,
		// when spawning behind the caster, at least 40 deg off it) so wisps fan out instead of flying
		// through the caster. Same rule as the original's SplineTranslateTo launch.
		float rel = WrapAngle(DirToHeading(toTarget) - a_casterHeading);
		if (!a_spawnBehind) {
			rel = std::clamp(rel, -_t.maxLaunchOffset, _t.maxLaunchOffset);
		} else if (std::fabs(rel) < _t.minLaunchOffsetBehind) {
			rel = rel < 0.0f ? -_t.minLaunchOffsetBehind : _t.minLaunchOffsetBehind;
		}
		const float launchHeading = a_casterHeading + rel;
		_heading = launchHeading;

		if (!a_world.targetValid || dist <= _t.approachRadius) {
			// Already on top of it: go straight to the picking dive.
			_phase = FlightPhase::kPick;
			_pickStart = _pos;
			return;
		}

		const float launchLen = std::min(_t.launchDistance, 0.5f * dist);
		_segControl = a_spawn + HeadingToDir(launchHeading) * launchLen + kUp * (_t.launchRise + Arc(dist));
		_phase = FlightPhase::kOutbound;
	}

	float Flight::Arc(float a_legLength) const noexcept
	{
		return std::clamp(a_legLength * _t.arcFactor, _t.minArc, _t.maxArc);
	}

	Vec3 Flight::ApproachPoint(const Vec3& a_target) const noexcept
	{
		// Stop short of the target on the side the wisp arrives from.
		Vec3 back = (_segControl - a_target);
		back.z = std::max(back.z, 0.0f);
		return a_target + back.Normalized() * _t.approachRadius;
	}

	Vec3 Flight::ReturnPoint(const FlightWorld& a_world) const noexcept
	{
		Vec3 away = _pos - a_world.casterPoint;
		away.z = 0.0f;
		if (away.Length2D() < 1.0f) {
			away = HeadingToDir(_heading);
		}
		return a_world.casterPoint + away.Normalized() * a_world.returnRadius;
	}

	float Flight::CurrentSpeed() const noexcept
	{
		if (_phase == FlightPhase::kOutbound && _phaseAge < _t.accelTime) {
			const float k = _t.accelTime > 0.0f ? _phaseAge / _t.accelTime : 1.0f;
			return (_t.launchSpeed + (_t.cruiseSpeed - _t.launchSpeed) * k) * _t.speedMult;
		}
		return _t.cruiseSpeed * _t.speedMult;
	}

	void Flight::MoveTowards(const Vec3& a_goal, float a_distance) noexcept
	{
		const Vec3  delta = a_goal - _pos;
		const float len = delta.Length();
		if (len <= a_distance || len < 1e-3f) {
			_pos = a_goal;
		} else {
			_pos += delta * (a_distance / len);
		}
	}

	void Flight::EnterReturn(const FlightWorld& a_world) noexcept
	{
		_phase = FlightPhase::kReturn;
		_phaseAge = 0.0f;
		_segStart = _pos;
		_s = 0.0f;
		const Vec3 end = ReturnPoint(a_world);
		_segControl = Lerp(_segStart, end, 0.5f) + kUp * Arc(Distance(_segStart, end));
	}

	FlightStep Flight::Update(float a_dt, const FlightWorld& a_world) noexcept
	{
		FlightStep step{};
		if (_phase == FlightPhase::kDone) {
			step.done = true;
			return step;
		}

		a_dt = std::clamp(a_dt, 0.0f, 0.25f);
		_age += a_dt;
		_phaseAge += a_dt;
		const Vec3 before = _pos;

		if (_age > _t.maxLifetime) {
			_phase = FlightPhase::kDone;
			step.done = true;
			step.timedOut = true;
			return step;
		}

		if (!a_world.targetValid && (_phase == FlightPhase::kOutbound || _phase == FlightPhase::kPick)) {
			EnterReturn(a_world);
		}

		switch (_phase) {
		case FlightPhase::kOutbound:
			{
				const Vec3  end = ApproachPoint(a_world.targetPoint);
				const float len = BezierLength(_segStart, _segControl, end);
				_s = std::min(1.0f, _s + CurrentSpeed() * a_dt / len);
				_pos = Bezier(_segStart, _segControl, end, _s);
				if (_s >= 1.0f) {
					_phase = FlightPhase::kPick;
					_phaseAge = 0.0f;
					_pickStart = _pos;
				}
			}
			break;
		case FlightPhase::kPick:
			MoveTowards(a_world.targetPoint, _t.pickSpeed * _t.speedMult * a_dt);
			if (Distance(_pos, a_world.targetPoint) <= _t.pickContact || _phaseAge >= _t.pickMaxTime) {
				step.harvestNow = true;
				_harvested = true;
				_phase = FlightPhase::kBackoff;
				_phaseAge = 0.0f;
			}
			break;
		case FlightPhase::kBackoff:
			MoveTowards(_pickStart, _t.backoffSpeed * _t.speedMult * a_dt);
			if (Distance(_pos, _pickStart) < 1.0f || _phaseAge >= _t.backoffMaxTime) {
				EnterReturn(a_world);
			}
			break;
		case FlightPhase::kReturn:
			{
				const Vec3  end = ReturnPoint(a_world);
				const float len = BezierLength(_segStart, _segControl, end);
				_s = std::min(1.0f, _s + CurrentSpeed() * a_dt / len);
				_pos = Bezier(_segStart, _segControl, end, _s);
				if (_s >= 1.0f || Distance(_pos, a_world.casterPoint) <= a_world.returnRadius) {
					_phase = FlightPhase::kDone;
					step.done = true;
				}
			}
			break;
		case FlightPhase::kDone:
			step.done = true;
			break;
		}

		const Vec3 moved = _pos - before;
		if (moved.Length2D() > 0.01f) {
			_heading = DirToHeading(moved);
		}
		return step;
	}
}
