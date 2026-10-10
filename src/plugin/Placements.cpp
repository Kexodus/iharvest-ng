#include "Placements.h"

namespace ihng::placements
{
	namespace
	{
		const Def kAll[] = {
#include "Placements.inl"
		};

		std::array<Resolved, std::size(kAll)> g_resolved{};

		template <class T>
		T* Find(std::string_view a_file, RE::FormID a_local)
		{
			if (a_file.empty()) {
				return nullptr;
			}
			return RE::TESDataHandler::GetSingleton()->LookupForm<T>(a_local, a_file);
		}
	}

	std::span<const Def> All() noexcept
	{
		return kAll;
	}

	void Resolve(std::string_view a_pluginName)
	{
		std::size_t ok = 0;
		for (std::size_t i = 0; i < std::size(kAll); ++i) {
			const auto& d = kAll[i];
			auto&       r = g_resolved[i];
			r.base = Find<RE::TESBoundObject>(a_pluginName, d.base);
			r.cell = Find<RE::TESObjectCELL>(d.cellFile, d.cellLocal);
			r.world = Find<RE::TESWorldSpace>(d.worldFile, d.worldLocal);
			r.owner = Find<RE::TESForm>(d.ownerFile, d.ownerLocal);
			if (r.base && r.cell && (d.worldFile.empty() || r.world)) {
				++ok;
			} else {
				logger::warn("Placements: #{} ({}|{:06X}) unresolved; it will not be placed", i, d.cellFile, d.cellLocal);
				r = {};
			}
		}
		logger::info("Placements: {}/{} spell tome / staff placements resolved", ok, std::size(kAll));
	}

	const Resolved& Get(std::size_t a_index) noexcept
	{
		return g_resolved[a_index];
	}
}
