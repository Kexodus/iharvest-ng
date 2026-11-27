#pragma once

#include "ihng/core/Text.h"

// Translated UI text (keys and English defaults: core/Text.cpp). Load() runs once on the main thread;
// the getters are safe from any thread, including the Menu Framework render thread.
namespace ihng::text
{
	// Reads Data/Interface/Translations/IHarvestNG_<sLanguage>.txt, falling back to the ENGLISH file
	// and then, per key, to the built-in English text. Call at kDataLoaded, before ui::Register().
	void Load();

	// The text for a_key; an unknown key is returned as is (and is a bug).
	[[nodiscard]] const char* Get(std::string_view a_key);

	// Get() + "##<key>": ImGui shows the text but keys the widget by the stable part, so translated
	// labels that happen to be equal still get distinct IDs.
	[[nodiscard]] const char* Label(std::string_view a_key);

	// The text for a_key with each "{}" replaced by the next argument (std::format "{}" of it).
	template <class... Args>
	[[nodiscard]] std::string Format(std::string_view a_key, const Args&... a_args)
	{
		const std::array<std::string, sizeof...(Args)>      values{ std::format("{}", a_args)... };
		std::array<std::string_view, sizeof...(Args)> views{};
		for (std::size_t i = 0; i < values.size(); ++i) {
			views[i] = values[i];
		}
		return core::text::Substitute(Get(a_key), views);
	}
}
