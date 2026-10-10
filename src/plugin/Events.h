#pragma once

namespace ihng::events
{
	// Registers the game event sinks. Sinks only filter cheaply and enqueue work for the Manager.
	void Register();
}
