#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

// Translatable UI text. Translations live in Data/Interface/Translations/IHarvestNG_<LANGUAGE>.txt,
// the standard Skyrim translation-file format: one "$KEY<TAB>text" per line.
namespace ihng::core::text
{
	struct Entry
	{
		std::string_view key;      // "$IHNG_..."
		std::string_view english;  // built-in text, used when a translation lacks the key
	};

	// Every translatable string of the DLL. The shipped IHarvestNG_ENGLISH.txt lists exactly these
	// (a unit test checks it), so translators can start from a complete file.
	[[nodiscard]] std::span<const Entry> Defaults() noexcept;

	using Table = std::unordered_map<std::string, std::string>;

	// Parses a translation file. Accepts UTF-16 LE with BOM (what SKSE and the game require) and UTF-8
	// (with or without BOM, for translators whose editor saves that). Keys start with '$' and end at the
	// first tab; the rest of the line (minus a trailing CR) is the text, returned as UTF-8. Other lines
	// are ignored; a later duplicate key wins.
	[[nodiscard]] Table Parse(std::span<const std::byte> a_file);

	// Replaces each "{}" in a_template, left to right, with the next argument. Missing arguments leave
	// the "{}" in place and extra ones are dropped. Nothing else is interpreted, so a broken
	// translation can garble a line but never crash the game the way a bad printf format would.
	[[nodiscard]] std::string Substitute(std::string_view a_template, std::span<const std::string_view> a_args);

	// UTF-8 encoding of a UTF-16 LE byte sequence (unpaired surrogates become U+FFFD).
	[[nodiscard]] std::string Utf16LeToUtf8(std::span<const std::byte> a_bytes);
}
