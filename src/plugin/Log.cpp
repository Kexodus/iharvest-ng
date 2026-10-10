#include "Log.h"

namespace ihng::log
{
	bool Init()
	{
		auto path = SKSE::log::log_directory();
		if (!path) {
			return false;
		}
		*path /= std::format("{}.log", SKSE::GetPluginName());

		std::vector<spdlog::sink_ptr> sinks{
			std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true),
			std::make_shared<spdlog::sinks::msvc_sink_mt>()
		};

		auto logger = std::make_shared<spdlog::logger>("iHarvestNG", sinks.begin(), sinks.end());
		logger->set_level(spdlog::level::info);
		logger->flush_on(spdlog::level::info);
		logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

		spdlog::set_default_logger(std::move(logger));
		return true;
	}

	void SetLevel(int a_level)
	{
		const auto level = (a_level >= spdlog::level::trace && a_level <= spdlog::level::off) ?
		                       static_cast<spdlog::level::level_enum>(a_level) :
		                       spdlog::level::info;
		spdlog::set_level(level);
		spdlog::flush_on(level < spdlog::level::info ? level : spdlog::level::info);
	}
}
