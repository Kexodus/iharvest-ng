#include "Forms.h"

namespace ihng
{
	Forms& Forms::Get() noexcept
	{
		static Forms forms;
		return forms;
	}

	namespace
	{
		template <class T>
		T* Lookup(std::string_view a_plugin, RE::FormID a_local, const char* a_what, bool& a_ok)
		{
			auto* form = RE::TESDataHandler::GetSingleton()->LookupForm<T>(a_local, a_plugin);
			if (!form) {
				logger::error("Forms: {} ({}|{:#05x}) not found or wrong type", a_what, a_plugin, a_local);
				a_ok = false;
			}
			return form;
		}
	}

	bool Forms::Resolve()
	{
		auto* dh = RE::TESDataHandler::GetSingleton();
		pluginName = {};
		for (auto name : kPluginNames) {
			if (dh->LookupModByName(name)) {
				pluginName = name;
				break;
			}
		}
		if (pluginName.empty()) {
			logger::critical("Forms: iHarvestNG.esp is not active. The spell, staff and tomes will not exist; "
							 "enable the plugin in your load order.");
			_ready = false;
			return false;
		}

		bool ok = true;
		harvestEffect = Lookup<RE::EffectSetting>(pluginName, 0x804, "IHNG_HarvestEffect", ok);
		harvestStaffEffect = Lookup<RE::EffectSetting>(pluginName, 0x8C6, "IHNG_HarvestStaffEffect", ok);
		harvestSpell = Lookup<RE::SpellItem>(pluginName, 0x800, "IHNG_HarvestSpell", ok);
		harvestStaffAbility = Lookup<RE::SpellItem>(pluginName, 0x8CB, "IHNG_HarvestStaffAbility", ok);
		harvestStaffEnch = Lookup<RE::EnchantmentItem>(pluginName, 0x8C7, "IHNG_HarvestStaffEnch", ok);
		harvestStaff = Lookup<RE::TESObjectWEAP>(pluginName, 0x8C5, "IHNG_HarvestStaff", ok);
		isHarvestStaff = Lookup<RE::BGSKeyword>(pluginName, 0x8CC, "IHNG_IsHarvestStaff", ok);
		spellTome = Lookup<RE::TESObjectBOOK>(pluginName, 0x819, "IHNG_SpellTomeHarvest", ok);
		wisp = Lookup<RE::TESObjectACTI>(pluginName, 0x8F1, "IHNG_Wisp", ok);
		wispSilent = Lookup<RE::TESObjectACTI>(pluginName, 0x8F2, "IHNG_WispSilent", ok);
		spawnPoof = Lookup<RE::BGSArtObject>(pluginName, 0x80E, "IHNG_FXGetterCritterSpawnPoof", ok);
		despawnSound = Lookup<RE::TESSound>(pluginName, 0x818, "IHNG_SFXGetterCritterDespawn", ok);
		alterationDualCasting = RE::TESForm::LookupByID<RE::BGSPerk>(0x000153CD);
		if (!alterationDualCasting) {
			logger::warn("Forms: vanilla AlterationDualCasting perk (000153CD) not found; double wisps disabled");
		}

		_ready = ok;
		logger::info("Forms: {} from {}", ok ? "all records resolved" : "MISSING RECORDS", pluginName);
		return ok;
	}
}
