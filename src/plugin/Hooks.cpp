#include "Hooks.h"

#include "Manager.h"

namespace ihng::hooks
{
	namespace
	{
		// A call inside Main::Update that runs once per frame on the main thread, on every runtime.
		// Same site used by many SE/AE/VR plugins (e.g. OpenAnimationReplacer, SpellHotbar2, VRFly).
		struct MainUpdate
		{
			static void thunk()
			{
				func();
				Manager::GetSingleton().Update();
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		std::uintptr_t CallSite()
		{
			const bool ae1130 = REL::Module::IsAE() && REL::Module::get().version() >= SKSE::RUNTIME_SSE_1_6_1130;
			REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(35565, 36564), REL::Relocate(0x748, ae1130 ? 0xC2B : 0xC26, 0x7EE) };
			return target.address();
		}
	}

	bool Install()
	{
		const auto site = CallSite();
		if (*reinterpret_cast<const std::uint8_t*>(site) != 0xE8) {
			logger::critical("Hooks: Main::Update call site at {:#x} is not a CALL (runtime {}); IHarvest NG disabled",
				site, REL::Module::get().version().string());
			return false;
		}

		SKSE::AllocTrampoline(14);
		auto& trampoline = SKSE::GetTrampoline();
		MainUpdate::func = trampoline.write_call<5>(site, MainUpdate::thunk);
		logger::info("Hooks: main update hook installed");
		return true;
	}
}
