#include "Serialization.h"

#include "Manager.h"

namespace ihng::serialization
{
	namespace
	{
		constexpr std::uint32_t kUniqueID = 'IHNG';
		constexpr std::uint32_t kTempRefs = 'TMPR';
		constexpr std::uint32_t kTomes = 'TOME';
		constexpr std::uint32_t kStats = 'STAT';
		constexpr std::uint32_t kVersion = 1;

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

		std::vector<RE::FormID> ReadIDs(SKSE::SerializationInterface* a_intfc)
		{
			std::uint32_t n = 0;
			a_intfc->ReadRecordData(n);
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
				if (version != kVersion) {
					logger::warn("Serialization: skipping record {:08X} with unknown version {}", type, version);
					continue;
				}
				switch (type) {
				case kTempRefs:
					m.SetLoadedRefsToDelete(ReadIDs(a_intfc));
					break;
				case kTomes:
					m.SetProcessedTomes(ReadIDs(a_intfc));
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
