#include "Text.h"

#include <fstream>

namespace ihng::text
{
	namespace
	{
		struct Item
		{
			std::string text;
			std::string label;  // text + "##" + key
		};

		// Keys are the string literals of core::text::Defaults(), so the views never dangle.
		using Items = std::unordered_map<std::string_view, Item>;

		Items Build(const core::text::Table* a_translation, std::size_t& a_translated)
		{
			Items items;
			a_translated = 0;
			for (const auto& entry : core::text::Defaults()) {
				std::string text(entry.english);
				if (a_translation) {
					if (const auto it = a_translation->find(std::string(entry.key)); it != a_translation->end() && !it->second.empty()) {
						text = it->second;
						++a_translated;
					}
				}
				std::string label = std::format("{}##{}", text, entry.key);
				items.emplace(entry.key, Item{ std::move(text), std::move(label) });
			}
			return items;
		}

		const Items& English()
		{
			static const Items items = [] {
				std::size_t unused = 0;
				return Build(nullptr, unused);
			}();
			return items;
		}

		// Published once by Load() and never freed: the render thread may still hold the old table.
		std::atomic<const Items*> g_items{ nullptr };

		const Items& Current()
		{
			const auto* items = g_items.load(std::memory_order_acquire);
			return items ? *items : English();
		}

		std::string GameLanguage()
		{
			std::string language = "ENGLISH";
			if (auto* ini = RE::INISettingCollection::GetSingleton()) {
				if (auto* setting = ini->GetSetting("sLanguage:General"); setting && setting->GetType() == RE::Setting::Type::kString && setting->data.s && *setting->data.s) {
					language = setting->data.s;
				}
			}
			std::ranges::transform(language, language.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
			return language;
		}

		std::optional<core::text::Table> ReadFile(const std::filesystem::path& a_path)
		{
			std::ifstream file(a_path, std::ios::binary);
			if (!file) {
				return std::nullopt;
			}
			const std::vector<char> bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
			return core::text::Parse(std::as_bytes(std::span(bytes)));
		}
	}

	namespace
	{
		// An unknown key (a bug) shows as itself. Copied per thread: a_key may be a temporary.
		const char* Missing(std::string_view a_key)
		{
			thread_local std::string copy;
			copy = a_key;
			return copy.c_str();
		}
	}

	void Load()
	{
		const auto language = GameLanguage();
		const auto path = [](std::string_view a_language) {
			return std::filesystem::path(std::format("Data/Interface/Translations/IHarvestNG_{}.txt", a_language));
		};

		auto table = ReadFile(path(language));
		auto used = path(language);
		if (!table && language != "ENGLISH") {
			table = ReadFile(path("ENGLISH"));
			used = path("ENGLISH");
		}

		std::size_t translated = 0;
		auto*       items = new Items(Build(table ? &*table : nullptr, translated));
		g_items.store(items, std::memory_order_release);

		const auto total = core::text::Defaults().size();
		if (!table) {
			logger::info("Text: no {} found for language {}; using built-in English", path(language).generic_string(), language);
		} else {
			logger::info("Text: {} of {} strings from {} (language {})", translated, total, used.generic_string(), language);
			if (translated < total) {
				logger::info("Text: {} string(s) missing from the file use the built-in English text", total - translated);
			}
		}
	}

	const char* Get(std::string_view a_key)
	{
		const auto& items = Current();
		const auto  it = items.find(a_key);
		return it != items.end() ? it->second.text.c_str() : Missing(a_key);
	}

	const char* Label(std::string_view a_key)
	{
		const auto& items = Current();
		const auto  it = items.find(a_key);
		return it != items.end() ? it->second.label.c_str() : Missing(a_key);
	}
}
