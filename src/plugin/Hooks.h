#pragma once

namespace ihng::hooks
{
	// Installs the per-frame hook: Main::Update where its call site is verified, otherwise (AE 1.7.99+)
	// PlayerCharacter::Update. Returns false only on VR with an unexpected call site - the plugin then
	// stays inert instead of crashing.
	bool Install();
}
