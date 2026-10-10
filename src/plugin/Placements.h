#pragma once

namespace ihng::placements
{
	// One of the original's hand-placed Spell Tomes / Staves of Harvest (see Placements.inl).
	struct Def
	{
		RE::FormID       base;        // local id in IHarvestNG.esp
		std::string_view cellFile;
		RE::FormID       cellLocal;
		std::string_view worldFile;   // empty for interiors
		RE::FormID       worldLocal;
		RE::NiPoint3     pos;
		RE::NiPoint3     rot;
		std::string_view ownerFile;   // empty = no owner
		RE::FormID       ownerLocal;
	};

	struct Resolved
	{
		RE::TESBoundObject* base{ nullptr };
		RE::TESObjectCELL*  cell{ nullptr };
		RE::TESWorldSpace*  world{ nullptr };
		RE::TESForm*        owner{ nullptr };
	};

	[[nodiscard]] std::span<const Def> All() noexcept;

	// Looks every placement's forms up (kDataLoaded). Placements whose cell is missing are skipped.
	void Resolve(std::string_view a_pluginName);
	[[nodiscard]] const Resolved& Get(std::size_t a_index) noexcept;
}
