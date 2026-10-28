#pragma once

#include <type_traits>

namespace ihng
{
	struct ActiveEffectState
	{
		bool active{ false };
		bool dual{ false };
	};

	// Use the engine's visitor on every runtime. In particular, VR has no BSSimpleList to read.
	// Keep all ActiveEffect access inside the callback (MagicTarget.h's lifetime contract).
	// The small template seam lets tests exercise this walk without loading the game.
	template <class VisitResult, class Actor, class MagicItem, class EffectSetting>
	[[nodiscard]] ActiveEffectState ReadActiveEffect(Actor* a_actor, MagicItem* a_spell, EffectSetting* a_base)
	{
		ActiveEffectState state;
		if (!a_actor || !a_base) {
			return state;
		}
		auto* target = a_actor->AsMagicTarget();
		if (!target) {
			return state;
		}
		// GetHandle enters the engine handle manager; do it before the VR visitor's list walk.
		const auto caster = a_actor->GetHandle();
		target->VisitActiveEffects([&](auto* a_effect) {
			using Flag = typename std::remove_pointer_t<decltype(a_effect)>::Flag;
			// The base effect identifies Harvest. Missing spell/caster provenance is not a mismatch.
			if (a_effect && a_effect->GetBaseObject() == a_base &&
				a_effect->flags.none(Flag::kInactive, Flag::kDispelled) &&
				(!a_effect->spell || !a_spell || a_effect->spell == a_spell) &&
				(!a_effect->caster || !caster || a_effect->caster == caster)) {
				state.active = true;
				state.dual = state.dual || a_effect->flags.any(Flag::kDual);
			}
			// A later matching hand may carry the dual flag; don't stop at the first single effect.
			return state.dual ? VisitResult::kStop : VisitResult::kContinue;
		});
		return state;
	}
}
