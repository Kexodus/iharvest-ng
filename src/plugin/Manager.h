#pragma once

#include "Harvestable.h"
#include "Settings.h"
#include "Wisp.h"

#include <ihng/core/CastTiming.h>

namespace ihng
{
	// Owns every running harvest cast ("session"), every wisp in flight, and the bookkeeping the
	// original spread over 8 quests: target claiming / ignore cache, the staff toggle, spell-tome
	// despawning, and orphan cleanup. All game work happens in Update() on the main thread; event
	// sinks and the UI only enqueue work.
	class Manager
	{
	public:
		[[nodiscard]] static Manager& GetSingleton() noexcept;

		// Per frame, main thread (called from the Main::Update hook).
		void Update();

		// Thread-safe producers.
		void QueueTask(std::function<void()> a_task);
		void WatchActor(RE::ActorHandle a_actor);  // an actor got one of our effects (NPC casters)

		// Main thread actions.
		void OnStaffCast(RE::Actor* a_caster);
		void OnStaffEquipChanged(RE::Actor* a_actor, bool a_equipped);
		void OnReferenceAttached(RE::TESObjectREFR* a_ref);
		void OnCellLoaded(RE::TESObjectCELL* a_cell);  // places pending spell tomes / staves
		void RecallAll();
		void ClearCaches();
		void ResetAll();  // recall + clear + delete orphans

		// Save game integration (main thread).
		void OnRevert();
		void OnPostLoad();
		void SetLoadedRefsToDelete(std::vector<RE::FormID> a_ids);
		void SetProcessedTomes(std::vector<RE::FormID> a_ids);
		void SetPlacedIndices(std::vector<std::uint32_t> a_indices);
		[[nodiscard]] std::vector<std::uint32_t> PlacedIndices() const;
		void SetTotals(std::uint64_t a_sent, std::uint64_t a_harvested);
		[[nodiscard]] std::vector<RE::FormID> LiveTempRefIDs() const;
		[[nodiscard]] std::vector<RE::FormID> ProcessedTomes() const;

		struct Stats
		{
			std::uint32_t activeWisps{ 0 };
			std::uint32_t activeCasters{ 0 };
			std::uint64_t totalSent{ 0 };
			std::uint64_t totalHarvested{ 0 };
			std::uint64_t sessionSent{ 0 };
			std::uint64_t sessionHarvested{ 0 };
			std::uint32_t learnedTypes{ 0 };
			std::uint32_t examinedTypes{ 0 };
			float         lastScanMs{ 0.0f };
			float         lastFrameMs{ 0.0f };
		};
		[[nodiscard]] Stats GetStats() const;  // thread-safe snapshot

	private:
		Manager() = default;

		struct Session
		{
			RE::ActorHandle                  caster;
			bool                             staff{ false };
			RE::ActorValue                   chargeAV{ RE::ActorValue::kNone };
			const core::CastTuning*          tuning{ nullptr };
			core::SpawnClock                 clock{ core::CastParams{} };
			float                            radius{ 0.0f };
			float                            cost{ 0.0f };
			std::vector<RE::ObjectRefHandle> cache;  // up to 8 nearest candidates, nearest last
			bool                             alive{ true };
			std::uint32_t                    sent{ 0 };
			std::uint32_t                    wispsPerRefire{ 1 };  // 2 when dual casting with the Dual Casting perk
			double                           startedAt{ 0.0 };
		};

		void RunQueuedTasks();
		void UpdateSessions(float a_dt);
		void UpdateWisps(float a_dt);
		void SyncSession(RE::Actor* a_actor, bool a_staff, RE::ActiveEffect* a_effect);
		void StartSession(RE::Actor* a_actor, bool a_staff, RE::ActiveEffect* a_effect);
		core::AttemptResult Attempt(Session& a_session, RE::Actor* a_caster);
		core::AttemptResult DispatchOne(Session& a_session, RE::Actor* a_caster, std::uint32_t a_index);
		RE::TESObjectREFR*  NextTarget(Session& a_session, RE::Actor* a_caster, harvest::Kind& a_kindOut);
		void                Rescan(Session& a_session, RE::Actor* a_caster);
		[[nodiscard]] bool  IsClaimedOrIgnored(RE::FormID a_id) const;
		void                Release(const Wisp& a_wisp);
		void                DeleteOrphansNow();
		void                Place(std::size_t a_index);
		void                PlaceInAttachedCells();
		void                UpdatePendingKeyframes(float a_dt);
		void                ApplyConfigChanges();

		// Active effect on a_actor whose base is a_effect (not dispelled, conditions true), or nullptr.
		static RE::ActiveEffect* FindActiveEffect(RE::Actor* a_actor, RE::EffectSetting* a_effect);

		std::mutex                         _queueLock;
		std::vector<std::function<void()>> _tasks;
		std::vector<RE::ActorHandle>       _pendingWatch;

		std::vector<RE::ActorHandle>          _watched;  // NPC casters (player is always checked)
		std::vector<Session>                  _sessions;
		std::vector<std::unique_ptr<Wisp>>    _wisps;
		std::unordered_set<RE::FormID>        _claimed;       // targets a wisp is currently fetching
		std::unordered_map<RE::FormID, float> _ignoredUntil;  // recently harvested / failed targets
		std::unordered_set<RE::FormID>        _processedTomes;
		std::unordered_set<std::uint32_t>     _placed;             // Placements.inl indices already placed in this save
		std::vector<std::pair<RE::FormID, float>> _pendingKeyframe;  // placed refs waiting for 3D to freeze them
		std::vector<RE::FormID>               _deleteOnNextUpdate;  // temp refs saved mid-flight

		Config              _config;
		std::uint32_t       _configRevision{ 0 };
		harvest::Rules      _rules;
		double              _clock{ 0.0 };  // seconds of unpaused game time since load
		std::mt19937        _rng{ std::random_device{}() };

		mutable std::mutex _statsLock;
		Stats              _stats;
	};
}
