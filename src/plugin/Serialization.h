#pragma once

namespace ihng::serialization
{
	// SKSE co-save: wisp/poof references alive at save time (deleted again on load, so a save made
	// mid-harvest never leaks references), spell tomes already rolled for despawning, lifetime stats.
	void Register();
}
