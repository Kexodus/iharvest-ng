#include "Serialization.h"

#include "Manager.h"
#include "Placements.h"

namespace ihng::serialization
{
	namespace
	{
		constexpr std::uint32_t kUniqueID = 'IHNG';
		constexpr std::uint32_t kTempRefs = 'TMPR';
		constexpr std::uint32_t kTomes = 'TOME';
		constexpr std::uint32_t kStats = 'STAT';
		constexpr std::uint32_t kPlaced = 'PLAC';
		constexpr std::uint32_t kVersion = 1;
		// PLAC v1 stored indices into Placements.inl (1.0.1 - 1.0.6); v2 stores each placement's stable key,
		// so regenerating or reordering the table can't map an old save onto the wrong placements.
		constexpr std::uint32_t kPlacedVersion = 2;

		// Element count of a record laid out as [u32 n][n x a_elemSize], clamped to what the record can
		// actually hold (a corrupt co-save must not make us allocate gigabytes).
		std::uint32_t ReadCount(SKSE::SerializationInterface* a_intfc, std::uint32_t a_length, std::uint32_t a_elemSize)
		{
			std::uint32_t n = 0;
			if (!a_intfc->ReadRecordData(n)) {
				return 0;
			}
			const std::uint32_t fits = a_length >= sizeof(n) ? (a_length - sizeof(n)) / a_elemSize : 0;
			if (n > fits) {
				logger::warn("Serialization: record claims {} entries but only holds {}; truncating", n, fits);
				n = fits;
			}
			return n;
		}

		void WriteIDs(SKSE::SerializationInterface* a_intfc, std::uint32_t a_type, const std::vector<RE::FormID>& a_ids)
		{
			if (!a_intfc->OpenRecord(a_type, kVersion)) {
				logger::error("Serialization: could not open record {:08X}", a_type);
				return;
			}
			const auto n = static_cast<std::uint32_t>(a_ids.size());
			a_intfc->WriteRecordData(n);
			for (const auto id : a_ids) {
				a_intfc->WriteRecordData(id);
			}
		}

		std::vector<RE::FormID> ReadIDs(SKSE::SerializationInterface* a_intfc, std::uint32_t a_length)
		{
			const auto              n = ReadCount(a_intfc, a_length, sizeof(RE::FormID));
			std::vector<RE::FormID> ids;
			ids.reserve(n);
			for (std::uint32_t i = 0; i < n; ++i) {
				RE::FormID id = 0;
				RE::FormID resolved = 0;
				if (a_intfc->ReadRecordData(id) && a_intfc->ResolveFormID(id, resolved)) {
					ids.push_back(resolved);
				}
			}
			return ids;
		}

		void Save(SKSE::SerializationInterface* a_intfc)
		{
			auto& m = Manager::GetSingleton();
			WriteIDs(a_intfc, kTempRefs, m.LiveTempRefIDs());
			WriteIDs(a_intfc, kTomes, m.ProcessedTomes());
			if (a_intfc->OpenRecord(kPlaced, kPlacedVersion)) {
				const auto placed = m.PlacedIndices();
				const auto defs = placements::All();
				a_intfc->WriteRecordData(static_cast<std::uint32_t>(placed.size()));
				for (const auto i : placed) {
					a_intfc->WriteRecordData(static_cast<std::uint32_t>(defs[i].key));
				}
			}
			if (a_intfc->OpenRecord(kStats, kVersion)) {
				const auto stats = m.GetStats();
				a_intfc->WriteRecordData(stats.totalSent);
				a_intfc->WriteRecordData(stats.totalHarvested);
			}
		}

		void Load(SKSE::SerializationInterface* a_intfc)
		{
			auto&         m = Manager::GetSingleton();
			std::uint32_t type = 0;
			std::uint32_t version = 0;
			std::uint32_t length = 0;
			while (a_intfc->GetNextRecordInfo(type, version, length)) {
				const bool known = type == kPlaced ? (version == 1 || version == kPlacedVersion) : version == kVersion;
				if (!known) {
					logger::warn("Serialization: skipping record {:08X} with unknown version {}", type, version);
					continue;
				}
				switch (type) {
				case kTempRefs:
					m.SetLoadedRefsToDelete(ReadIDs(a_intfc, length));
					break;
				case kTomes:
					m.SetProcessedTomes(ReadIDs(a_intfc, length));
					break;
				case kPlaced:
					{
						const auto                 n = ReadCount(a_intfc, length, sizeof(std::uint32_t));
						const auto                 total = placements::All().size();
						std::vector<std::uint32_t> placed;
						placed.reserve(n);
						for (std::uint32_t k = 0; k < n; ++k) {
							std::uint32_t v = 0;
							if (!a_intfc->ReadRecordData(v)) {
								break;
							}
							if (version == 1) {
								// Old saves: indices into the table as it was through 1.0.6 (same order as now).
								if (v < total) {
									placed.push_back(v);
								}
							} else if (const auto i = placements::IndexOfKey(v)) {
								placed.push_back(static_cast<std::uint32_t>(*i));
							}
						}
						m.SetPlacedIndices(std::move(placed));
					}
					break;
				case kStats:
					{
						std::uint64_t sent = 0;
						std::uint64_t harvested = 0;
						a_intfc->ReadRecordData(sent);
						a_intfc->ReadRecordData(harvested);
						m.SetTotals(sent, harvested);
					}
					break;
				default:
					logger::warn("Serialization: unknown record {:08X}", type);
					break;
				}
			}
		}

		void Revert(SKSE::SerializationInterface*)
		{
			Manager::GetSingleton().OnRevert();
		}
	}

	void Register()
	{
		auto* s = SKSE::GetSerializationInterface();
		s->SetUniqueID(kUniqueID);
		s->SetSaveCallback(Save);
		s->SetLoadCallback(Load);
		s->SetRevertCallback(Revert);
	}
}
