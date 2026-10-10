#include "ihng/core/CastTiming.h"

#include <algorithm>
#include <cmath>

namespace ihng::core
{
	CastParams ComputeCastParams(const CastTuning& t, std::int32_t alt, std::int32_t alch, bool dual) noexcept
	{
		alt = std::max(alt, 0);
		alch = std::max(alch, 0);

		CastParams p{};
		p.delayUs = t.maxDelayUs;
		std::int32_t minDelay = t.minDelayBaseUs - t.minDelayPerAlteration * alt - t.minDelayPerAlchemy * alch;
		float accel = 0.85f - static_cast<float>(t.accelPerAlteration * alt + t.accelPerAlchemy * alch) / 1'000'000.0f;
		std::int32_t radius = t.maxRadiusBase + t.radiusPerAlteration * alt + t.radiusPerAlchemy * alch;

		if (dual) {
			radius = radius * t.dualCastRadiusPct / 100;
			accel = accel * static_cast<float>(t.dualCastAccelPct) / 100.0f;
			minDelay = minDelay * t.dualCastMinDelayPct / 100;
		}

		p.minDelayUs = std::clamp(minDelay, kAbsoluteMinDelayUs, t.maxDelayUs);
		p.accel = std::clamp(accel, 0.05f, 0.99f);
		p.radius = static_cast<float>(std::max(radius, 0));
		return p;
	}

	float ComputeWispCost(const CastTuning& t, float base, float alterationModPct) noexcept
	{
		// Like the original: only a lower bound (a negative AlterationMod makes wisps pricier).
		const float mod = std::max((100.0f - alterationModPct) / 100.0f, t.minCostFraction);
		return std::max(base, 0.0f) * mod;
	}

	SpawnClock::SpawnClock(const CastParams& a_params) noexcept :
		_delayUs(a_params.delayUs),
		_minDelayUs(a_params.minDelayUs),
		_accel(a_params.accel)
	{}

	bool SpawnClock::Tick(double a_dt) noexcept
	{
		if (!_started) {
			// OnEffectStart: RegisterForSingleUpdate(maxDelay) then DoCast() right away.
			_started = true;
			_untilNext = _delayUs / 1'000'000.0;
			return true;
		}

		_untilNext -= a_dt;
		if (_untilNext > 0.0) {
			return false;
		}

		// OnUpdate: accelerate, compute this interval (stretched by slowMode), then DoCast().
		if (_delayUs != _minDelayUs) {
			_delayUs = static_cast<std::int32_t>(static_cast<float>(_delayUs) * _accel);
			_delayUs = std::max(_delayUs, _minDelayUs);
		}
		double interval = _delayUs / 1'000'000.0;
		if (_slowMode > 0) {
			interval *= std::pow(static_cast<double>(_slowMode), 0.66);
		}
		_untilNext = interval;
		return true;
	}

	void SpawnClock::Report(AttemptResult a_result) noexcept
	{
		switch (a_result) {
		case AttemptResult::kDispatched:
			_slowMode = 0;
			break;
		case AttemptResult::kNothingFound:
			_slowMode = std::min(_slowMode + 1, 64);
			break;
		case AttemptResult::kAtCap:
			_slowMode = std::min(_slowMode + 3, 64);
			break;
		case AttemptResult::kNeutral:
			break;
		}
	}
}
