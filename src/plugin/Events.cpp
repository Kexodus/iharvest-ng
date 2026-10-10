#include "Events.h"

#include "Forms.h"
#include "Manager.h"

namespace ihng::events
{
	namespace
	{
		using Control = RE::BSEventNotifyControl;

		RE::FormID IDOf(const RE::TESForm* a_form) { return a_form ? a_form->GetFormID() : 0; }

		// NPC casters: the player is polled every frame anyway; NPCs are only watched once one of our
		// effects lands on them, so idle NPCs cost nothing.
		struct MagicEffectApply final : RE::BSTEventSink<RE::TESMagicEffectApplyEvent>
		{
			Control ProcessEvent(const RE::TESMagicEffectApplyEvent* a_event, RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override
			{
				if (!a_event || !a_event->target || !Forms::IsReady()) {
					return Control::kContinue;
				}
				const auto& forms = Forms::Get();
				if (a_event->magicEffect != IDOf(forms.harvestEffect) && a_event->magicEffect != IDOf(forms.harvestStaffEffect)) {
					return Control::kContinue;
				}
				if (auto* actor = a_event->target->As<RE::Actor>(); actor && actor != RE::PlayerCharacter::GetSingleton()) {
					Manager::GetSingleton().WatchActor(actor->GetHandle());
				}
				return Control::kContinue;
			}
		};

		struct SpellCast final : RE::BSTEventSink<RE::TESSpellCastEvent>
		{
			Control ProcessEvent(const RE::TESSpellCastEvent* a_event, RE::BSTEventSource<RE::TESSpellCastEvent>*) override
			{
				if (!a_event || !a_event->object || !Forms::IsReady() || a_event->spell != IDOf(Forms::Get().harvestStaffEnch)) {
					return Control::kContinue;
				}
				if (auto* actor = a_event->object->As<RE::Actor>()) {
					Manager::GetSingleton().QueueTask([handle = actor->GetHandle()] {
						if (const auto a = handle.get()) {
							Manager::GetSingleton().OnStaffCast(a.get());
						}
					});
				}
				return Control::kContinue;
			}
		};

		struct Equip final : RE::BSTEventSink<RE::TESEquipEvent>
		{
			Control ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
			{
				if (!a_event || !a_event->actor || !Forms::IsReady()) {
					return Control::kContinue;
				}
				auto* base = RE::TESForm::LookupByID(a_event->baseObject);
				const auto& forms = Forms::Get();
				auto* weap = base ? base->As<RE::TESObjectWEAP>() : nullptr;
				if (!weap || !(weap == forms.harvestStaff || (forms.isHarvestStaff && weap->HasKeyword(forms.isHarvestStaff)))) {
					return Control::kContinue;
				}
				if (auto* actor = a_event->actor->As<RE::Actor>()) {
					Manager::GetSingleton().QueueTask([handle = actor->GetHandle(), equipped = a_event->equipped] {
						if (const auto a = handle.get()) {
							Manager::GetSingleton().OnStaffEquipChanged(a.get(), equipped);
						}
					});
				}
				return Control::kContinue;
			}
		};

		struct CellAttach final : RE::BSTEventSink<RE::TESCellAttachDetachEvent>
		{
			Control ProcessEvent(const RE::TESCellAttachDetachEvent* a_event, RE::BSTEventSource<RE::TESCellAttachDetachEvent>*) override
			{
				if (!a_event || !a_event->attached || !a_event->reference || !Forms::IsReady()) {
					return Control::kContinue;
				}
				const auto& forms = Forms::Get();
				auto*       base = a_event->reference->GetBaseObject();
				if (!base || !(base == forms.spellTome || base == forms.wisp || base == forms.wispSilent || base == forms.spawnPoof)) {
					return Control::kContinue;
				}
				Manager::GetSingleton().QueueTask([handle = a_event->reference->GetHandle()] {
					if (const auto ref = handle.get()) {
						Manager::GetSingleton().OnReferenceAttached(ref.get());
					}
				});
				return Control::kContinue;
			}
		};
	}

	void Register()
	{
		auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
		if (!holder) {
			logger::error("Events: ScriptEventSourceHolder unavailable");
			return;
		}
		static MagicEffectApply magicEffectApply;
		static SpellCast        spellCast;
		static Equip            equip;
		static CellAttach       cellAttach;
		holder->AddEventSink<RE::TESMagicEffectApplyEvent>(&magicEffectApply);
		holder->AddEventSink<RE::TESSpellCastEvent>(&spellCast);
		holder->AddEventSink<RE::TESEquipEvent>(&equip);
		holder->AddEventSink<RE::TESCellAttachDetachEvent>(&cellAttach);
		logger::info("Events: sinks registered");
	}
}
