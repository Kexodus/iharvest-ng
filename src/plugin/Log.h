#pragma once

namespace ihng::log
{
	// Creates My Games/<edition>/SKSE/IHarvestNG.log (truncated each launch) as the default logger.
	bool Init();

	// spdlog level: 0 trace .. 6 off. Out-of-range values fall back to info.
	void SetLevel(int a_level);
}
