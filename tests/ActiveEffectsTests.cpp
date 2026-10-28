#include "../src/plugin/ActiveEffects.h"

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <stdexcept>
#include <vector>

namespace
{
	// Only the API surface used by the production reader. No CommonLib/engine initialization.
	enum class Result { kContinue, kStop };
	struct Spell {};
	struct BaseEffect {};
	struct Effect
	{
		enum class Flag : unsigned { kDual = 1, kInactive = 2, kDispelled = 4 };
		struct Flags
		{
			unsigned bits{};
			bool any(Flag a_flag) const { return (bits & static_cast<unsigned>(a_flag)) != 0; }
			bool none(Flag a_first, Flag a_second) const { return !any(a_first) && !any(a_second); }
		};
		Spell*      spell{};
		BaseEffect* base{};
		unsigned    caster{};
		Flags       flags{};
		BaseEffect* GetBaseObject() const { return base; }
	};
	struct Target
	{
		std::vector<Effect*> effects;
		unsigned visits{};
		bool visiting{ false };
		// Like VR, the flat list is unavailable. Using it must not silently disable harvesting.
		std::vector<Effect*>* GetActiveEffectList() const { return nullptr; }
		void VisitActiveEffects(std::function<Result(Effect*)> a_visit)
		{
			++visits;
			visiting = true;
			for (auto* effect : effects) {
				if (a_visit(effect) == Result::kStop) {
					break;
				}
			}
			visiting = false;
		}
	};
	struct Actor
	{
		Target*  target{};
		unsigned handle{ 42 };
		Target*  AsMagicTarget() const { return target; }
		unsigned GetHandle() const
		{
			if (target && target->visiting) {
				throw std::logic_error("Handle acquisition must happen before the effect visitor");
			}
			return handle;
		}
	};
}

TEST_CASE("Harvest effects use the visitor when the VR flat list is unavailable", "[effects]")
{
	Spell spell;
	BaseEffect base;
	Effect effect{ &spell, &base, 42 };
	Target target{ { nullptr, &effect } };
	Actor actor{ &target };
	REQUIRE(target.GetActiveEffectList() == nullptr);
	const auto state = ihng::ReadActiveEffect<Result>(&actor, &spell, &base);
	CHECK(state.active);
	CHECK_FALSE(state.dual);
	CHECK(target.visits == 1);
	// The result is still usable once the engine has unlinked/changed the effect.
	target.effects.clear();
	effect = {};
	CHECK(state.active);
	CHECK_FALSE(ihng::ReadActiveEffect<Result>(&actor, &spell, &base).active);
}

TEST_CASE("Harvest rejects known foreign provenance, wrong base and inactive flags", "[effects]")
{
	Spell spell, otherSpell;
	BaseEffect base, otherBase;
	Effect effect{ &spell, &base, 42 };
	Target target{ { &effect } };
	Actor actor{ &target };
	SECTION("wrong spell reusing the same base") { effect.spell = &otherSpell; }
	SECTION("wrong base on the same spell") { effect.base = &otherBase; }
	SECTION("another caster") { effect.caster = 99; }
	SECTION("inactive") { effect.flags.bits = static_cast<unsigned>(Effect::Flag::kInactive); }
	SECTION("dispelled") { effect.flags.bits = static_cast<unsigned>(Effect::Flag::kDispelled); }
	CHECK_FALSE(ihng::ReadActiveEffect<Result>(&actor, &spell, &base).active);
}

TEST_CASE("Missing spell or caster provenance preserves base-effect matching", "[effects]")
{
	Spell spell;
	Spell* expected = &spell;
	BaseEffect base;
	Effect effect{ &spell, &base, 42 };
	Target target{ { &effect } };
	Actor actor{ &target };
	SECTION("effect spell missing") { effect.spell = nullptr; }
	SECTION("effect caster missing") { effect.caster = 0; }
	SECTION("both missing") { effect.spell = nullptr; effect.caster = 0; }
	SECTION("expected spell unavailable") { expected = nullptr; }
	SECTION("actor handle unavailable") { actor.handle = 0; }
	CHECK(ihng::ReadActiveEffect<Result>(&actor, expected, &base).active);
}

TEST_CASE("Known foreign provenance is rejected even when the other field is missing", "[effects]")
{
	Spell spell, otherSpell;
	BaseEffect base;
	Effect effect{ &spell, &base, 42 };
	Target target{ { &effect } };
	Actor actor{ &target };
	SECTION("foreign spell without caster") { effect.spell = &otherSpell; effect.caster = 0; }
	SECTION("foreign caster without spell") { effect.spell = nullptr; effect.caster = 99; }
	CHECK_FALSE(ihng::ReadActiveEffect<Result>(&actor, &spell, &base).active);
}

TEST_CASE("Staff ability identity is distinct and does not require a casting handle", "[effects]")
{
	Spell spell, ability, enchantment;
	BaseEffect spellBase, staffBase;
	Effect cast{ &spell, &spellBase, 42 };
	Effect staff{ &ability, &staffBase, 0 };
	Target target{ { &cast, &staff } };
	Actor actor{ &target };
	CHECK(ihng::ReadActiveEffect<Result>(&actor, &spell, &spellBase).active);
	CHECK(ihng::ReadActiveEffect<Result>(&actor, &ability, &staffBase).active);
	staff.caster = 99;
	CHECK_FALSE(ihng::ReadActiveEffect<Result>(&actor, &ability, &staffBase).active);
	staff.caster = actor.handle;
	CHECK(ihng::ReadActiveEffect<Result>(&actor, &ability, &staffBase).active);
	staff.spell = &enchantment;
	CHECK_FALSE(ihng::ReadActiveEffect<Result>(&actor, &ability, &staffBase).active);
	CHECK(ihng::ReadActiveEffect<Result>(&actor, &spell, &spellBase).active);
}

TEST_CASE("Dual flag is copied from any matching hand without retaining effects", "[effects]")
{
	Spell spell, otherSpell;
	BaseEffect base;
	Effect single{ &spell, &base, 42 };
	Effect dual{ &spell, &base, 42, { static_cast<unsigned>(Effect::Flag::kDual) } };
	Effect unrelated{ &otherSpell, &base, 42, dual.flags };
	Target target{ { &unrelated, &single } };
	Actor actor{ &target };
	CHECK_FALSE(ihng::ReadActiveEffect<Result>(&actor, &spell, &base).dual);
	target.effects.push_back(&dual);
	const auto state = ihng::ReadActiveEffect<Result>(&actor, &spell, &base);
	dual = {};
	CHECK(state.active);
	CHECK(state.dual);
}

TEST_CASE("Missing actor, target or base effect cannot start a harvest session", "[effects]")
{
	Spell spell;
	BaseEffect base;
	Target target;
	Actor actor{ &target };
	CHECK_FALSE(ihng::ReadActiveEffect<Result>(static_cast<Actor*>(nullptr), &spell, &base).active);
	CHECK_FALSE(ihng::ReadActiveEffect<Result>(&actor, &spell, static_cast<BaseEffect*>(nullptr)).active);
	CHECK(target.visits == 0);
	actor.target = nullptr;
	CHECK_FALSE(ihng::ReadActiveEffect<Result>(&actor, &spell, &base).active);
}
