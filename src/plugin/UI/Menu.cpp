#include "UI/Menu.h"

#include "Forms.h"
#include "Manager.h"
#include "Settings.h"
#include "Text.h"

#include <IHarvestNG/Version.h>

// The vendored header expects <filesystem> first and pulls in <windows.h>; keep it to this TU.
#include <filesystem>

#include <SKSEMenuFramework.h>

namespace ihng::ui
{
	namespace
	{
		namespace ImGui = ImGuiMCP;

		std::atomic<bool> g_dirty{ false };

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

		// Every control takes a translation key (core/Text.cpp); its tooltip is "<key>_Tip".
		void TipFor(std::string_view a_key)
		{
			if (ImGui::IsItemHovered()) {
				const auto tip = std::string(a_key) + "_Tip";
				ImGui::SetTooltip("%s", text::Get(tip));
			}
		}

		bool Check(std::string_view a_key, bool& a_value)
		{
			const bool changed = ImGui::Checkbox(text::Label(a_key), &a_value);
			TipFor(a_key);
			return changed;
		}

		bool SliderF(std::string_view a_key, float& a_value, float a_min, float a_max, const char* a_fmt)
		{
			const bool changed = ImGui::SliderFloat(text::Label(a_key), &a_value, a_min, a_max, a_fmt);
			TipFor(a_key);
			return changed;
		}

		bool SliderI(std::string_view a_key, std::int32_t& a_value, int a_min, int a_max)
		{
			int v = a_value;
			const bool changed = ImGui::SliderInt(text::Label(a_key), &v, a_min, a_max);
			TipFor(a_key);
			a_value = v;
			return changed;
		}

		void Action(std::string_view a_key, std::function<void()> a_task)
		{
			if (ImGui::Button(text::Label(a_key))) {
				Manager::GetSingleton().QueueTask(std::move(a_task));
			}
			TipFor(a_key);
		}

		void Line(const std::string& a_text)
		{
			ImGui::TextUnformatted(a_text.c_str());
		}

		void __stdcall RenderSettings()
		{
			ImGui::SeparatorText(text::Get("$IHNG_HeaderCasting"));
			Edit([](Config& c) {
				bool ch = false;
				ch |= SliderF("$IHNG_MagickaPerWisp", c.magickaCostPerWisp, 0.0f, 100.0f, "%.2f");
				ch |= SliderF("$IHNG_ChargePerWisp", c.staffChargePerWisp, 0.0f, 100.0f, "%.2f");
				ch |= SliderF("$IHNG_XpPerWisp", c.xpPerWisp, 0.0f, 100.0f, "%.2f");
				ch |= SliderI("$IHNG_MaxWisps", c.maxWisps, 1, 128);
				ch |= SliderF("$IHNG_RadiusMult", c.radiusMult, 0.1f, 4.0f, "%.2f");
				ch |= Check("$IHNG_DualCast", c.dualCastPerkDoubleWisps);
				ch |= Check("$IHNG_NpcCasters", c.allowNPCCasters);
				return ch;
			});

			ImGui::SeparatorText(text::Get("$IHNG_HeaderHarvest"));
			Edit([](Config& c) {
				bool ch = false;
				ch |= Check("$IHNG_Food", c.harvestFood);
				ch |= Check("$IHNG_Hearthfire", c.harvestHearthfire);
				ch |= Check("$IHNG_Critters", c.harvestCritters);
				ch |= Check("$IHNG_Arrows", c.harvestArrows);
				ch |= Check("$IHNG_Ownership", c.respectOwnership);
				return ch;
			});

			ImGui::SeparatorText(text::Get("$IHNG_HeaderWisps"));
			Edit([](Config& c) {
				bool ch = false;
				ch |= SliderF("$IHNG_SpawnDistance", c.spawnDistanceMult, -3.0f, 3.0f, "%.1f");
				ch |= SliderF("$IHNG_ReturnDistance", c.returnOffset, 192.0f, 1000.0f, "%.0f");
				ch |= SliderF("$IHNG_FlightSpeed", c.speedMult, 0.25f, 20.0f, "%.2f");
				ch |= Check("$IHNG_Sounds", c.sounds);
				return ch;
			});
		}

		void __stdcall RenderStatus()
		{
			const auto s = Manager::GetSingleton().GetStats();
			Line(std::format("{} {} - {}", version::kName, version::kString, text::Get(Forms::IsReady() ? "$IHNG_Ready" : "$IHNG_NotActive")));
			if (const auto& warning = Forms::Get().WorldOverrideWarning(); !warning.empty()) {
				const auto line = text::Format("$IHNG_Warning", warning);
				ImGui::TextWrapped("%s", line.c_str());
			}
			ImGui::Separator();
			Line(text::Format("$IHNG_WispsInFlight", s.activeWisps));
			Line(text::Format("$IHNG_CastersHarvesting", s.activeCasters));
			Line(text::Format("$IHNG_WispsSent", s.sessionSent, s.totalSent));
			Line(text::Format("$IHNG_ThingsHarvested", s.sessionHarvested, s.totalHarvested));

			ImGui::SeparatorText(text::Get("$IHNG_HeaderMaintenance"));
			Action("$IHNG_RecallAll", [] { Manager::GetSingleton().RecallAll(); });
			ImGui::SameLine();
			Action("$IHNG_Reset", [] { Manager::GetSingleton().ResetAll(); });

			if (ImGui::CollapsingHeader(text::Label("$IHNG_HeaderDiagnostics"))) {
				Line(text::Format("$IHNG_TypesLearned", s.learnedTypes, s.examinedTypes));
				Line(text::Format("$IHNG_Timings", std::format("{:.3f}", s.lastScanMs), std::format("{:.3f}", s.lastFrameMs)));
				Edit([](Config& c) {
					static const char* levels[] = { "trace", "debug", "info", "warn", "error", "critical", "off" };
					int lvl = c.logLevel;
					const bool ch = ImGui::Combo(text::Label("$IHNG_LogLevel"), &lvl, levels, 7);
					TipFor("$IHNG_LogLevel");
					c.logLevel = lvl;
					return ch;
				});
			}

			if (ImGui::CollapsingHeader(text::Label("$IHNG_HeaderCheats"))) {
				Action("$IHNG_LearnSpell", [] {
					if (auto* spell = Forms::Get().harvestSpell) {
						RE::PlayerCharacter::GetSingleton()->AddSpell(spell);
					}
				});
				ImGui::SameLine();
				Action("$IHNG_AddStaff", [] {
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
		SKSEMenuFramework::SetSection("IHarvest NG");
		SKSEMenuFramework::AddSectionItem(text::Get("$IHNG_PageStatus"), RenderStatus);
		SKSEMenuFramework::AddSectionItem(text::Get("$IHNG_PageSettings"), RenderSettings);
		static auto* event = SKSEMenuFramework::AddEvent(OnMenuEvent, 0.0f);
		(void)event;
		logger::info("UI: SKSE Menu Framework v{:.2f} page registered", SKSEMenuFramework::GetMenuFrameworkVersion());
	}
}
