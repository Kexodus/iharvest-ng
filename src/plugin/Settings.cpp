#include "Settings.h"

#include <SimpleIni.h>

namespace ihng
{
	void Config::Clamp() noexcept
	{
		logLevel = std::clamp(logLevel, 0, 6);
		magickaCostPerWisp = std::clamp(magickaCostPerWisp, 0.0f, 500.0f);
		staffChargePerWisp = std::clamp(staffChargePerWisp, 0.0f, 500.0f);
		xpPerWisp = std::clamp(xpPerWisp, 0.0f, 100.0f);
		maxWisps = std::clamp(maxWisps, 1, 128);
		radiusMult = std::clamp(radiusMult, 0.1f, 4.0f);
		spawnDistanceMult = std::clamp(spawnDistanceMult, -3.0f, 3.0f);
		returnOffset = std::clamp(returnOffset, 192.0f, 1000.0f);
		speedMult = std::clamp(speedMult, 0.25f, 20.0f);
		readTomeDespawnChance = std::clamp(readTomeDespawnChance, 0.0f, 1.0f);
	}

	Settings& Settings::GetSingleton() noexcept
	{
		static Settings singleton;
		return singleton;
	}

	namespace
	{
		template <class T>
		void Read(CSimpleIniA& a_ini, const char* a_section, const char* a_key, T& a_value)
		{
			if constexpr (std::is_same_v<T, bool>) {
				a_value = a_ini.GetBoolValue(a_section, a_key, a_value);
			} else if constexpr (std::is_floating_point_v<T>) {
				a_value = static_cast<T>(a_ini.GetDoubleValue(a_section, a_key, a_value));
			} else {
				a_value = static_cast<T>(a_ini.GetLongValue(a_section, a_key, static_cast<long>(a_value)));
			}
		}

		template <class T>
		void Write(CSimpleIniA& a_ini, const char* a_section, const char* a_key, const T& a_value, const char* a_comment)
		{
			if constexpr (std::is_same_v<T, bool>) {
				a_ini.SetBoolValue(a_section, a_key, a_value, a_comment);
			} else if constexpr (std::is_floating_point_v<T>) {
				a_ini.SetDoubleValue(a_section, a_key, a_value, a_comment);
			} else {
				a_ini.SetLongValue(a_section, a_key, static_cast<long>(a_value), a_comment);
			}
		}

		// One table drives both reading and writing so the two can never drift apart.
		template <class Fn>
		void ForEachKey(Config& c, Fn&& fn)
		{
			fn("General", "iLogLevel", c.logLevel, "; 0 trace, 1 debug, 2 info, 3 warn, 4 error, 5 critical, 6 off");

			fn("Casting", "fMagickaCostPerWisp", c.magickaCostPerWisp, "; Magicka drained per wisp sent (spell). Reduced by Alteration cost reduction, never below 15%.");
			fn("Casting", "fStaffChargePerWisp", c.staffChargePerWisp, "; Enchantment charge drained per wisp sent (Staff of Harvest).");
			fn("Casting", "fXPPerWisp", c.xpPerWisp, "; Alteration skill-use XP per wisp sent (player only).");
			fn("Casting", "iMaxWisps", c.maxWisps, "; Maximum wisps in flight at once (1-128).");
			fn("Casting", "fRadiusMult", c.radiusMult, "; Multiplier on the skill-based search radius (1.0 = original).");
			fn("Casting", "bAllowNPCCasters", c.allowNPCCasters, "; NPCs who cast Harvest / use the staff send wisps too.");
			fn("Casting", "bDualCastPerkDoubleWisps", c.dualCastPerkDoubleWisps, "; Dual casting with the Alteration Dual Casting perk sends two wisps at a time (each costs magicka).");

			fn("Harvest", "bFood", c.harvestFood, "; Also harvest loose food and plants that only yield food.");
			fn("Harvest", "bHearthfire", c.harvestHearthfire, "; Harvest Hearthfire garden planters.");
			fn("Harvest", "bCritters", c.harvestCritters, "; Catch butterflies, moths, dragonflies, fish, and harvest nirnroot, poison blooms etc.");
			fn("Harvest", "bArrows", c.harvestArrows, "; Also collect loose and recoverable arrows/bolts.");
			fn("Harvest", "bRespectOwnership", c.respectOwnership, "; Never take anything that would count as stealing.");

			fn("Wisps", "fSpawnDistanceMult", c.spawnDistanceMult, "; -3..3. Wisps appear this far in front of you (x192 units); negative = behind you.");
			fn("Wisps", "fReturnOffset", c.returnOffset, "; How close to you (192-1000 units) wisps return before vanishing.");
			fn("Wisps", "fSpeedMult", c.speedMult, "; Wisp flight speed multiplier (0.25-20).");
			fn("Wisps", "bSounds", c.sounds, "; Wisp hum and despawn sound.");

			fn("SpellTomes", "fReadTomeDespawnChance", c.readTomeDespawnChance, "; Chance that a placed Spell Tome: Harvest disappears once you already know the spell.");
		}
	}

	void Settings::Load()
	{
		CSimpleIniA ini;
		ini.SetUnicode();
		const auto rc = ini.LoadFile(std::string(kPath).c_str());

		Config c;
		ForEachKey(c, [&](const char* s, const char* k, auto& v, const char*) { Read(ini, s, k, v); });
		c.Clamp();
		{
			std::scoped_lock l(_lock);
			_config = c;
		}
		_revision.fetch_add(1, std::memory_order_acq_rel);

		if (rc < 0) {
			logger::info("Settings: {} not found, using defaults (writing a fresh one)", kPath);
			Save();
		} else {
			logger::info("Settings: loaded {}", kPath);
		}
	}

	void Settings::Save() const
	{
		Config c = Get();
		CSimpleIniA ini;
		ini.SetUnicode();
		ForEachKey(c, [&](const char* s, const char* k, auto& v, const char* comment) { Write(ini, s, k, v, comment); });
		if (ini.SaveFile(std::string(kPath).c_str()) < 0) {
			logger::warn("Settings: could not write {}", kPath);
		}
	}

	Config Settings::Get() const
	{
		std::scoped_lock l(_lock);
		return _config;
	}

	void Settings::Set(const Config& a_config)
	{
		Config c = a_config;
		c.Clamp();
		{
			std::scoped_lock l(_lock);
			_config = c;
		}
		_revision.fetch_add(1, std::memory_order_acq_rel);
	}
}
