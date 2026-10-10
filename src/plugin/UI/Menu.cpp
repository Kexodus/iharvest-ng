#include "UI/Menu.h"

#include "Forms.h"
#include "Manager.h"
#include "Settings.h"

#include <iHarvestNG/Version.h>

// The vendored header expects <filesystem> first and pulls in <windows.h>; keep it to this TU.
#include <filesystem>

#include <SKSEMenuFramework.h>

namespace ihng::ui
{
	namespace
	{
		namespace ImGui = ImGuiMCP;

		std::atomic<bool> g_dirty{ false };

		void Tooltip(const char* a_text)
		{
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s", a_text);
			}
		}

		// Edit-in-place helpers: apply live, persist when the menu closes.
		template <class Fn>
		void Edit(Fn&& a_fn)
		{
			auto c = Settings::GetSingleton().Get();
			if (a_fn(c)) {
				Settings::GetSingleton().Set(c);
				g_dirty = true;
			}
		}

		bool Check(const char* a_label, bool& a_value, const char* a_tip)
		{
			const bool changed = ImGui::Checkbox(a_label, &a_value);
			Tooltip(a_tip);
			return changed;
		}

		bool SliderF(const char* a_label, float& a_value, float a_min, float a_max, const char* a_fmt, const char* a_tip)
		{
			const bool changed = ImGui::SliderFloat(a_label, &a_value, a_min, a_max, a_fmt);
			Tooltip(a_tip);
			return changed;
		}

		bool SliderI(const char* a_label, std::int32_t& a_value, int a_min, int a_max, const char* a_tip)
		{
			int v = a_value;
			const bool changed = ImGui::SliderInt(a_label, &v, a_min, a_max);
			Tooltip(a_tip);
			a_value = v;
			return changed;
		}

		void Action(const char* a_label, const char* a_tip, std::function<void()> a_task)
		{
			if (ImGui::Button(a_label)) {
				Manager::GetSingleton().QueueTask(std::move(a_task));
			}
			Tooltip(a_tip);
		}

		void __stdcall RenderSettings()
		{
			ImGui::SeparatorText("Casting");
			Edit([](Config& c) {
				bool ch = false;
				ch |= SliderF("Magicka per wisp (spell)", c.magickaCostPerWisp, 0.0f, 100.0f, "%.2f", "Magicka drained for every wisp sent. Alteration cost reduction applies (never below 15%).");
				ch |= SliderF("Charge per wisp (staff)", c.staffChargePerWisp, 0.0f, 100.0f, "%.2f", "Enchantment charge drained for every wisp the Staff of Harvest sends.");
				ch |= SliderF("Alteration XP per wisp", c.xpPerWisp, 0.0f, 100.0f, "%.2f", "Skill-use experience granted for every wisp you send.");
				ch |= SliderI("Max wisps at once", c.maxWisps, 1, 128, "Concurrent wisps in flight across all casters.");
				ch |= SliderF("Search radius multiplier", c.radiusMult, 0.1f, 4.0f, "%.2f", "Scales the skill-based search radius (1.00 = original).");
				ch |= Check("Dual Casting perk: double wisps", c.dualCastPerkDoubleWisps, "Dual casting Harvest with the Alteration Dual Casting perk sends two wisps at a time (each costs magicka). Applies from your next cast.");
				ch |= Check("NPC casters", c.allowNPCCasters, "NPCs who cast Harvest or use the Staff of Harvest send wisps too.");
				return ch;
			});

			ImGui::SeparatorText("What to harvest");
			Edit([](Config& c) {
				bool ch = false;
				ch |= Check("Food", c.harvestFood, "Loose food, and plants that only yield food.");
				ch |= Check("Hearthfire planters", c.harvestHearthfire, "Your Hearthfire garden planters.");
				ch |= Check("Critters & special plants", c.harvestCritters, "Butterflies, moths, dragonflies, fish, nirnroot, poison blooms, Creation Club harvestables...");
				ch |= Check("Arrows & bolts", c.harvestArrows, "Loose ammo and recoverable arrows/bolts stuck in the world.");
				ch |= Check("Respect ownership", c.respectOwnership, "Never take anything that would count as stealing.");
				return ch;
			});

			ImGui::SeparatorText("Wisps");
			Edit([](Config& c) {
				bool ch = false;
				ch |= SliderF("Spawn distance multiplier", c.spawnDistanceMult, -3.0f, 3.0f, "%.1f", "Wisps appear this far in front of you (x192 units, further while running). Negative = behind you.");
				ch |= SliderF("Return distance", c.returnOffset, 192.0f, 1000.0f, "%.0f", "How close to you (units) wisps come back before vanishing.");
				ch |= SliderF("Flight speed multiplier", c.speedMult, 0.25f, 20.0f, "%.2f", "Wisp travel speed (1.00 = original 1000 units/s). Ctrl+click to type an exact value.");
				ch |= Check("Sounds", c.sounds, "Wisp hum and despawn sound.");
				return ch;
			});
		}

		void __stdcall RenderStatus()
		{
			const auto s = Manager::GetSingleton().GetStats();
			ImGui::Text("%s %s - %s", version::kName.data(), version::kString.data(), Forms::IsReady() ? "ready" : "PLUGIN NOT ACTIVE (enable iHarvestNG.esp)");
			ImGui::Separator();
			ImGui::Text("Wisps in flight: %u", s.activeWisps);
			ImGui::Text("Casters harvesting: %u", s.activeCasters);
			ImGui::Text("Wisps sent: %llu this session, %llu this save", s.sessionSent, s.totalSent);
			ImGui::Text("Things harvested: %llu this session, %llu this save", s.sessionHarvested, s.totalHarvested);

			ImGui::SeparatorText("Maintenance");
			Action("Recall all wisps", "Sends every wisp in flight home immediately.", [] { Manager::GetSingleton().RecallAll(); });
			ImGui::SameLine();
			Action("Reset", "Recall all wisps, forget learned harvestable types (re-examined automatically), and delete any stray wisp references in loaded cells. Use after changing your load order.",
				[] { Manager::GetSingleton().ResetAll(); });

			if (ImGui::CollapsingHeader("Diagnostics")) {
				ImGui::Text("Harvestable types learned: %u (of %u examined)", s.learnedTypes, s.examinedTypes);
				ImGui::Text("Last search: %.3f ms   Last frame cost: %.3f ms", s.lastScanMs, s.lastFrameMs);
				Edit([](Config& c) {
					static const char* levels[] = { "trace", "debug", "info", "warn", "error", "critical", "off" };
					int lvl = c.logLevel;
					const bool ch = ImGui::Combo("Log level", &lvl, levels, 7);
					Tooltip("Written to Documents/My Games/Skyrim Special Edition/SKSE/iHarvestNG.log");
					c.logLevel = lvl;
					return ch;
				});
			}

			if (ImGui::CollapsingHeader("Cheats / testing")) {
				Action("Learn Harvest spell", "Adds the Harvest spell to the player.", [] {
					if (auto* spell = Forms::Get().harvestSpell) {
						RE::PlayerCharacter::GetSingleton()->AddSpell(spell);
					}
				});
				ImGui::SameLine();
				Action("Add Staff of Harvest", "Adds a Staff of Harvest to the player's inventory.", [] {
					if (auto* staff = Forms::Get().harvestStaff) {
						RE::PlayerCharacter::GetSingleton()->AddObjectToContainer(staff, nullptr, 1, nullptr);
					}
				});
			}
		}

		void __stdcall OnMenuEvent(SKSEMenuFramework::Model::EventType a_type)
		{
			if (a_type == SKSEMenuFramework::Model::kCloseMenu && g_dirty.exchange(false)) {
				Settings::GetSingleton().Save();
			}
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logger::info("UI: SKSE Menu Framework not installed; configure via {}", Settings::kPath);
			return;
		}
		SKSEMenuFramework::SetSection("iHarvest NG");
		SKSEMenuFramework::AddSectionItem("Status & Maintenance", RenderStatus);
		SKSEMenuFramework::AddSectionItem("Settings", RenderSettings);
		static auto* event = SKSEMenuFramework::AddEvent(OnMenuEvent, 0.0f);
		(void)event;
		logger::info("UI: SKSE Menu Framework v{:.2f} page registered", SKSEMenuFramework::GetMenuFrameworkVersion());
	}
}
