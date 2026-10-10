#pragma once

namespace ihng
{
	// Every user-tunable value. Defaults reproduce the original IHarvest MCM defaults where one existed.
	struct Config
	{
		// [General]
		std::int32_t logLevel{ 2 };  // spdlog: 0 trace .. 6 off

		// [Casting]
		float        magickaCostPerWisp{ 8.0f };    // IH_MagickaDrainPerSpawn
		float        staffChargePerWisp{ 12.0f };   // IH_StaffDrainPerSpawn
		float        xpPerWisp{ 10.0f };            // IH_CastExp (Alteration skill-use points per wisp)
		std::int32_t maxWisps{ 64 };                // IH_CritterCap (concurrent)
		float        radiusMult{ 1.0f };            // NG: scales the skill-based search radius
		bool         allowNPCCasters{ true };       // NPCs who know the spell / wield the staff can harvest too
		bool         dualCastPerkDoubleWisps{ true };  // NG: dual casting with Alteration Dual Casting sends 2 wisps per refire

		// [Harvest]
		bool harvestFood{ false };        // IH_LearnFood: loose food and flora that only yields food
		bool harvestHearthfire{ true };   // IH_LearnHearthfire: Hearthfire planters (works natively in NG)
		bool harvestCritters{ true };     // butterflies, moths, dragonflies, fish, nirnroot, poison blooms...
		bool harvestArrows{ false };      // NG: loose arrows/bolts and recoverable stuck arrows
		bool respectOwnership{ true };    // never take anything that would be stealing

		// [Wisps]
		float spawnDistanceMult{ 1.0f };  // IH_SpawnDistanceMult (-3..3, negative = behind the caster)
		float returnOffset{ 192.0f };     // IH_OffsetReturnPoint (the original never despawned closer than 192)
		float speedMult{ 1.0f };          // NG: wisp travel speed multiplier
		bool  sounds{ true };             // wisp hum + despawn sound

		// [SpellTomes]
		float readTomeDespawnChance{ 0.8f };  // IH_SpellbookScript readDespawnChance

		void Clamp() noexcept;
	};

	// Data/SKSE/Plugins/iHarvestNG.ini. Thread-safe: the game loop reads snapshots, the Menu
	// Framework page (render thread) edits and saves.
	class Settings
	{
	public:
		static constexpr auto kPath = "Data/SKSE/Plugins/iHarvestNG.ini"sv;

		[[nodiscard]] static Settings& GetSingleton() noexcept;

		void Load();
		void Save() const;

		[[nodiscard]] Config Get() const;
		void                 Set(const Config& a_config);  // clamps, stores, bumps Revision()

		// Incremented on every Set(); lets the game loop cheaply detect changes.
		[[nodiscard]] std::uint32_t Revision() const noexcept { return _revision.load(std::memory_order_acquire); }

	private:
		Settings() = default;

		mutable std::mutex         _lock;
		Config                     _config;
		std::atomic<std::uint32_t> _revision{ 1 };
	};
}
