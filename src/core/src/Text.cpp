#include "ihng/core/Text.h"

#include <array>
#include <cstdint>

namespace ihng::core::text
{
	namespace
	{
		// Order = order in IHarvestNG_ENGLISH.txt. "_Tip" keys are the hover tooltips of the control
		// with the same name. "{}" marks a value the plugin fills in.
		constexpr std::array kDefaults{
			// Menu Framework pages
			Entry{ "$IHNG_PageStatus", "Status & Maintenance" },
			Entry{ "$IHNG_PageSettings", "Settings" },

			// Settings: Casting
			Entry{ "$IHNG_HeaderCasting", "Casting" },
			Entry{ "$IHNG_MagickaPerWisp", "Magicka per wisp (spell)" },
			Entry{ "$IHNG_MagickaPerWisp_Tip", "Magicka drained for every wisp sent. Alteration cost reduction applies (never below 15%)." },
			Entry{ "$IHNG_ChargePerWisp", "Charge per wisp (staff)" },
			Entry{ "$IHNG_ChargePerWisp_Tip", "Enchantment charge drained for every wisp the Staff of Harvest sends." },
			Entry{ "$IHNG_XpPerWisp", "Alteration XP per wisp" },
			Entry{ "$IHNG_XpPerWisp_Tip", "Skill-use experience granted for every wisp you send." },
			Entry{ "$IHNG_MaxWisps", "Max wisps at once" },
			Entry{ "$IHNG_MaxWisps_Tip", "Concurrent wisps in flight across all casters." },
			Entry{ "$IHNG_RadiusMult", "Search radius multiplier" },
			Entry{ "$IHNG_RadiusMult_Tip", "Scales the skill-based search radius (1.00 = original)." },
			Entry{ "$IHNG_DualCast", "Dual Casting perk: double wisps" },
			Entry{ "$IHNG_DualCast_Tip", "Dual casting Harvest with the Alteration Dual Casting perk sends two wisps at a time (each costs magicka). Applies from your next cast." },
			Entry{ "$IHNG_NpcCasters", "NPC casters" },
			Entry{ "$IHNG_NpcCasters_Tip", "NPCs who cast Harvest or use the Staff of Harvest send wisps too." },

			// Settings: What to harvest
			Entry{ "$IHNG_HeaderHarvest", "What to harvest" },
			Entry{ "$IHNG_Food", "Food" },
			Entry{ "$IHNG_Food_Tip", "Loose food, and plants that only yield food." },
			Entry{ "$IHNG_Hearthfire", "Hearthfire planters" },
			Entry{ "$IHNG_Hearthfire_Tip", "Your Hearthfire garden planters, and planters added by other mods." },
			Entry{ "$IHNG_Critters", "Critters & special plants" },
			Entry{ "$IHNG_Critters_Tip", "Butterflies, moths, dragonflies, fish, nirnroot, poison blooms, Creation Club harvestables..." },
			Entry{ "$IHNG_Arrows", "Arrows & bolts" },
			Entry{ "$IHNG_Arrows_Tip", "Loose ammo and recoverable arrows/bolts stuck in the world." },
			Entry{ "$IHNG_Ownership", "Respect ownership" },
			Entry{ "$IHNG_Ownership_Tip", "Never take anything that would count as stealing." },

			// Settings: Wisps
			Entry{ "$IHNG_HeaderWisps", "Wisps" },
			Entry{ "$IHNG_SpawnDistance", "Spawn distance multiplier" },
			Entry{ "$IHNG_SpawnDistance_Tip", "Wisps appear this far in front of you (x192 units, further while running). Negative = behind you." },
			Entry{ "$IHNG_ReturnDistance", "Return distance" },
			Entry{ "$IHNG_ReturnDistance_Tip", "How close to you (units) wisps come back before vanishing." },
			Entry{ "$IHNG_FlightSpeed", "Flight speed multiplier" },
			Entry{ "$IHNG_FlightSpeed_Tip", "Wisp travel speed (1.00 = original 1000 units/s). Ctrl+click to type an exact value." },
			Entry{ "$IHNG_Sounds", "Sounds" },
			Entry{ "$IHNG_Sounds_Tip", "Wisp hum and despawn sound." },

			// Status page
			Entry{ "$IHNG_Ready", "ready" },
			Entry{ "$IHNG_NotActive", "PLUGIN NOT ACTIVE (enable IHarvestNG.esp)" },
			Entry{ "$IHNG_Warning", "WARNING: {}" },
			Entry{ "$IHNG_WispsInFlight", "Wisps in flight: {}" },
			Entry{ "$IHNG_CastersHarvesting", "Casters harvesting: {}" },
			Entry{ "$IHNG_WispsSent", "Wisps sent: {} this session, {} this save" },
			Entry{ "$IHNG_ThingsHarvested", "Things harvested: {} this session, {} this save" },
			Entry{ "$IHNG_HeaderMaintenance", "Maintenance" },
			Entry{ "$IHNG_RecallAll", "Recall all wisps" },
			Entry{ "$IHNG_RecallAll_Tip", "Sends every wisp in flight home immediately." },
			Entry{ "$IHNG_Reset", "Reset" },
			Entry{ "$IHNG_Reset_Tip", "Recall all wisps, forget learned harvestable types (re-examined automatically), and delete any stray wisp references in loaded cells. Use after changing your load order." },
			Entry{ "$IHNG_HeaderDiagnostics", "Diagnostics" },
			Entry{ "$IHNG_TypesLearned", "Harvestable types learned: {} (of {} examined)" },
			Entry{ "$IHNG_Timings", "Last search: {} ms   Last frame cost: {} ms" },
			Entry{ "$IHNG_LogLevel", "Log level" },
			Entry{ "$IHNG_LogLevel_Tip", "Written to Documents/My Games/Skyrim Special Edition/SKSE/IHarvestNG.log" },
			Entry{ "$IHNG_HeaderCheats", "Cheats / testing" },
			Entry{ "$IHNG_LearnSpell", "Learn Harvest spell" },
			Entry{ "$IHNG_LearnSpell_Tip", "Adds the Harvest spell to the player." },
			Entry{ "$IHNG_AddStaff", "Add Staff of Harvest" },
			Entry{ "$IHNG_AddStaff_Tip", "Adds a Staff of Harvest to the player's inventory." },

			// Message box (also shown on the status page) when an old IHarvest plugin breaks the world map
			Entry{ "$IHNG_WorldOverride", "IHarvest NG: {} overrides the Tamriel worldspace, which breaks the world map. IHarvestNG.esp 1.0.1+ never does - this is an old copy (1.0.0, or the original IHarvest / a fork). In MO2, remove or disable the old mod so only the current IHarvest NG provides IHarvestNG.esp." },
		};

		void AppendUtf8(std::string& a_out, std::uint32_t a_cp)
		{
			if (a_cp < 0x80) {
				a_out += static_cast<char>(a_cp);
			} else if (a_cp < 0x800) {
				a_out += static_cast<char>(0xC0 | (a_cp >> 6));
				a_out += static_cast<char>(0x80 | (a_cp & 0x3F));
			} else if (a_cp < 0x10000) {
				a_out += static_cast<char>(0xE0 | (a_cp >> 12));
				a_out += static_cast<char>(0x80 | ((a_cp >> 6) & 0x3F));
				a_out += static_cast<char>(0x80 | (a_cp & 0x3F));
			} else {
				a_out += static_cast<char>(0xF0 | (a_cp >> 18));
				a_out += static_cast<char>(0x80 | ((a_cp >> 12) & 0x3F));
				a_out += static_cast<char>(0x80 | ((a_cp >> 6) & 0x3F));
				a_out += static_cast<char>(0x80 | (a_cp & 0x3F));
			}
		}

		void ParseLine(std::string_view a_line, Table& a_out)
		{
			if (!a_line.empty() && a_line.back() == '\r') {
				a_line.remove_suffix(1);
			}
			if (a_line.size() < 2 || a_line.front() != '$') {
				return;
			}
			const auto tab = a_line.find('\t');
			if (tab == std::string_view::npos || tab < 2) {
				return;
			}
			a_out.insert_or_assign(std::string(a_line.substr(0, tab)), std::string(a_line.substr(tab + 1)));
		}
	}

	std::span<const Entry> Defaults() noexcept
	{
		return kDefaults;
	}

	std::string Utf16LeToUtf8(std::span<const std::byte> a_bytes)
	{
		std::string out;
		out.reserve(a_bytes.size());
		const auto unit = [&](std::size_t i) {
			return static_cast<std::uint32_t>(a_bytes[i]) | (static_cast<std::uint32_t>(a_bytes[i + 1]) << 8);
		};
		const std::size_t n = a_bytes.size() & ~std::size_t{ 1 };
		for (std::size_t i = 0; i < n; i += 2) {
			std::uint32_t cp = unit(i);
			if (cp >= 0xD800 && cp <= 0xDBFF) {
				if (i + 2 < n) {  // n is even, so the next unit is complete
					const std::uint32_t lo = unit(i + 2);
					if (lo >= 0xDC00 && lo <= 0xDFFF) {
						cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
						i += 2;
					} else {
						cp = 0xFFFD;
					}
				} else {
					cp = 0xFFFD;
				}
			} else if (cp >= 0xDC00 && cp <= 0xDFFF) {
				cp = 0xFFFD;
			}
			AppendUtf8(out, cp);
		}
		return out;
	}

	Table Parse(std::span<const std::byte> a_file)
	{
		std::string utf8;
		if (a_file.size() >= 2 && a_file[0] == std::byte{ 0xFF } && a_file[1] == std::byte{ 0xFE }) {
			utf8 = Utf16LeToUtf8(a_file.subspan(2));
		} else {
			auto body = a_file;
			if (body.size() >= 3 && body[0] == std::byte{ 0xEF } && body[1] == std::byte{ 0xBB } && body[2] == std::byte{ 0xBF }) {
				body = body.subspan(3);
			}
			utf8.assign(reinterpret_cast<const char*>(body.data()), body.size());
		}

		Table table;
		std::string_view rest(utf8);
		while (!rest.empty()) {
			const auto nl = rest.find('\n');
			ParseLine(rest.substr(0, nl), table);
			if (nl == std::string_view::npos) {
				break;
			}
			rest.remove_prefix(nl + 1);
		}
		return table;
	}

	std::string Substitute(std::string_view a_template, std::span<const std::string_view> a_args)
	{
		std::string out;
		out.reserve(a_template.size() + 16);
		auto arg = a_args.begin();
		std::size_t i = 0;
		while (i < a_template.size()) {
			if (arg != a_args.end() && a_template[i] == '{' && i + 1 < a_template.size() && a_template[i + 1] == '}') {
				out += *arg++;
				i += 2;
			} else {
				out += a_template[i++];
			}
		}
		return out;
	}
}
