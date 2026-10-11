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
			// Untyped lookup + As<T>(): the typed LookupForm<T> only accepts an exact T::FORMTYPE, which
			// silently rejects every form for base classes like TESBoundObject / TESForm (1.0.1-1.0.2 bug).
			auto* form = RE::TESDataHandler::GetSingleton()->LookupForm(a_local, a_file);
			return form ? form->As<T>() : nullptr;
		}
	}

	std::span<const Def> All() noexcept
	{
		return kAll;
	}

	std::optional<std::size_t> IndexOfKey(RE::FormID a_key) noexcept
	{
		for (std::size_t i = 0; i < std::size(kAll); ++i) {
			if (kAll[i].key == a_key) {
				return i;
			}
		}
		return std::nullopt;
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
