#pragma once

namespace ihng::hooks
{
	// Installs the per-frame main-loop hook (Main::Update). Returns false if the call site does not
	// look like what we expect (unknown runtime) - the plugin then stays inert instead of crashing.
	bool Install();
}
