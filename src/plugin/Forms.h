#pragma once

namespace ihng
{
	// Records from IHarvestNG.esp (ESL-flagged). Resolved once at kDataLoaded.
	struct Forms
	{
		static constexpr std::array kPluginNames{ "IHarvestNG.esp"sv, "IHarvestNG.esl"sv };

		RE::EffectSetting*   harvestEffect{ nullptr };       // concentration spell effect
		RE::EffectSetting*   harvestStaffEffect{ nullptr };  // staff ability (toggled) effect
		RE::SpellItem*       harvestSpell{ nullptr };
		RE::SpellItem*       harvestStaffAbility{ nullptr };
		RE::EnchantmentItem* harvestStaffEnch{ nullptr };
		RE::TESObjectWEAP*   harvestStaff{ nullptr };
		RE::BGSKeyword*      isHarvestStaff{ nullptr };
		RE::TESObjectBOOK*   spellTome{ nullptr };
		RE::TESObjectACTI*   wisp{ nullptr };
		RE::TESObjectACTI*   wispSilent{ nullptr };       // same wisp without the looping hum
		RE::BGSArtObject*    spawnPoof{ nullptr };          // no longer spawned; only used to clean up old refs
		RE::TESSound*        despawnSound{ nullptr };
		RE::BGSPerk*         alterationDualCasting{ nullptr };  // vanilla 0x000153CD (optional)

		std::string_view pluginName;

		[[nodiscard]] static Forms& Get() noexcept;
		[[nodiscard]] static bool   IsReady() noexcept { return Get()._ready; }

		// Returns false (and logs) when the plugin is missing or a record could not be found.
		bool Resolve();

		// Finds IHarvest-family plugins that override the Tamriel worldspace (the 1.0.0 IHarvestNG.esp,
		// the original IHarvest.esl / its forks) - that override reverts world map mods. Empty = none.
		void CheckWorldOverrides();
		[[nodiscard]] const std::string& WorldOverrideWarning() const noexcept { return _worldOverrideWarning; }

	private:
		bool        _ready{ false };
		std::string _worldOverrideWarning;
	};
}
