#pragma once

// In-game configuration through SKSE Menu Framework (soft dependency). Without it, edit
// Data/SKSE/Plugins/iHarvestNG.ini instead - every setting lives there too.
namespace ihng::ui
{
	// Call at kPostLoad (all SKSE plugins loaded). No-op when the framework is not installed.
	void Register();
}
