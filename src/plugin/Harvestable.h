#pragma once

namespace ihng::harvest
{
	// Which optional categories are enabled (from Config).
	struct Rules
	{
		bool food{ false };
		bool hearthfire{ true };
		bool critters{ true };
		bool arrows{ false };

		friend bool operator==(const Rules&, const Rules&) = default;
	};

	enum class Kind : std::uint8_t
	{
		kNone,       // not something wisps harvest
		kProduce,    // TREE / FLOR that yields an ingredient (or food / Hearthfire produce, per Rules)
		kLooseItem,  // ingredient, food or ammo lying in the world
		kScripted,   // scripted harvestable activator: critters, nirnroot, poison bloom, CC harvestables...
		kArrow,      // recoverable arrow/bolt projectile
	};

	// Classifies a reference's base object. Results are cached per base form (like the original's
	// IH_LearnedTypes / IH_ExaminedTypes form lists), so each base is only ever inspected once.
	// Scripted activators are classified from the first reference seen, exactly like the original.
	// Main thread only.
	[[nodiscard]] Kind Classify(RE::TESObjectREFR* a_ref, const Rules& a_rules);

	// Forgets everything learned (MCM "Clear flora cache" equivalent). Also called when Rules change.
	void ClearCache();
	[[nodiscard]] std::size_t LearnedCount();
	[[nodiscard]] std::size_t ExaminedCount();

	// Cheap per-reference checks: loaded, enabled, not already harvested, not activation-blocked
	// (activators exempt, as in the original), and - if requested - taking it would not be a crime.
	[[nodiscard]] bool IsAvailable(RE::TESObjectREFR* a_ref, Kind a_kind, RE::Actor* a_caster, bool a_respectOwnership);

	// Harvests a_ref on behalf of a_caster through the game's own activation path, so vanilla
	// behaviour (Green Thumb, harvest sounds, notifications, critter/nirnroot/Hearthfire scripts,
	// quest hooks) applies exactly as if the caster had picked it by hand.
	bool Harvest(RE::TESObjectREFR* a_ref, Kind a_kind, RE::Actor* a_caster);

	// The point a wisp flies into: the centre of the reference's world bound.
	[[nodiscard]] RE::NiPoint3 TargetPoint(RE::TESObjectREFR* a_ref);
}
