#include "ihng/core/Text.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

using namespace ihng::core::text;

namespace
{
	std::vector<std::byte> Bytes(std::string_view a_s)
	{
		const auto* p = reinterpret_cast<const std::byte*>(a_s.data());
		return { p, p + a_s.size() };
	}

	// UTF-16 LE with BOM, from a UTF-8 literal holding only BMP characters.
	std::vector<std::byte> Utf16(std::u16string_view a_s)
	{
		std::vector<std::byte> out{ std::byte{ 0xFF }, std::byte{ 0xFE } };
		for (const char16_t c : a_s) {
			out.push_back(static_cast<std::byte>(c & 0xFF));
			out.push_back(static_cast<std::byte>(c >> 8));
		}
		return out;
	}

	std::string Sub(std::string_view a_template, std::initializer_list<std::string_view> a_args)
	{
		const std::vector<std::string_view> args(a_args);
		return Substitute(a_template, args);
	}
}

TEST_CASE("Parse reads UTF-16 LE translation files like SKSE does")
{
	const auto table = Parse(Utf16(u"$IHNG_A\t收割\r\n; comment\r\nnot a key\tx\r\n$IHNG_B\tWisps: {}\r\n$\tshort\r\n$IHNG_NoTab\r\n"));
	REQUIRE(table.size() == 2);
	CHECK(table.at("$IHNG_A") == "\xE6\x94\xB6\xE5\x89\xB2");  // 收割 in UTF-8
	CHECK(table.at("$IHNG_B") == "Wisps: {}");
}

TEST_CASE("Parse also accepts UTF-8, with or without BOM, and LF line ends")
{
	const auto plain = Parse(Bytes("$IHNG_A\tOne\n$IHNG_B\tTwo"));
	CHECK(plain.at("$IHNG_A") == "One");
	CHECK(plain.at("$IHNG_B") == "Two");

	const auto bom = Parse(Bytes("\xEF\xBB\xBF$IHNG_A\t\xE6\x94\xB6\r\n"));
	CHECK(bom.at("$IHNG_A") == "\xE6\x94\xB6");
}

TEST_CASE("Parse: the first tab splits key and text; later duplicates win")
{
	const auto table = Parse(Bytes("$IHNG_A\tx\ty\n$IHNG_A\tz\tw\n"));
	CHECK(table.at("$IHNG_A") == "z\tw");
}

TEST_CASE("Utf16LeToUtf8 handles surrogate pairs and broken input")
{
	// U+1F33F (herb) = D83C DF3F; a lone high surrogate and a lone low one become U+FFFD.
	const std::array<std::byte, 8> pair{ std::byte{ 0x3C }, std::byte{ 0xD8 }, std::byte{ 0x3F }, std::byte{ 0xDF },
		std::byte{ 0x3C }, std::byte{ 0xD8 }, std::byte{ 0x3F }, std::byte{ 0xDF } };
	CHECK(Utf16LeToUtf8(pair) == "\xF0\x9F\x8C\xBF\xF0\x9F\x8C\xBF");

	const std::array<std::byte, 5> broken{ std::byte{ 0x3C }, std::byte{ 0xD8 }, std::byte{ 0x41 }, std::byte{ 0x00 }, std::byte{ 0x41 } };
	CHECK(Utf16LeToUtf8(broken) == "\xEF\xBF\xBD" "A");  // odd trailing byte dropped

	const std::array<std::byte, 2> lowOnly{ std::byte{ 0x00 }, std::byte{ 0xDC } };
	CHECK(Utf16LeToUtf8(lowOnly) == "\xEF\xBF\xBD");
}

TEST_CASE("Substitute fills {} left to right and tolerates bad translations")
{
	CHECK(Sub("Sent: {} now, {} total", { "3", "40" }) == "Sent: 3 now, 40 total");
	CHECK(Sub("{} / {}", { "a" }) == "a / {}");          // missing argument stays visible
	CHECK(Sub("no slots", { "a", "b" }) == "no slots");  // extra arguments dropped
	CHECK(Sub("%s %d {", { "a" }) == "%s %d {");         // printf codes and stray braces are plain text
	CHECK(Sub("{}{}", { "x", "y" }) == "xy");
}

TEST_CASE("String table: unique $IHNG_ keys, every _Tip belongs to a control")
{
	std::set<std::string_view> keys;
	for (const auto& e : Defaults()) {
		CHECK(e.key.starts_with("$IHNG_"));
		CHECK(!e.english.empty());
		CHECK(e.english.find('\t') == std::string_view::npos);
		CHECK(keys.insert(e.key).second);
	}
	for (const auto key : keys) {
		if (key.ends_with("_Tip")) {
			CHECK(keys.contains(key.substr(0, key.size() - 4)));
		}
	}
}

TEST_CASE("The shipped IHarvestNG_ENGLISH.txt matches the built-in table (run tools/make_translation.py)")
{
	std::ifstream file(IHNG_SOURCE_DIR "/dist/Data/Interface/Translations/IHarvestNG_ENGLISH.txt", std::ios::binary);
	REQUIRE(file);
	const std::vector<char> raw{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	REQUIRE(raw.size() >= 2);
	CHECK(static_cast<unsigned char>(raw[0]) == 0xFF);  // UTF-16 LE BOM, which SKSE requires
	CHECK(static_cast<unsigned char>(raw[1]) == 0xFE);

	const auto table = Parse(std::as_bytes(std::span(raw)));
	CHECK(table.size() == Defaults().size());
	for (const auto& e : Defaults()) {
		const auto it = table.find(std::string(e.key));
		REQUIRE(it != table.end());
		CHECK(it->second == e.english);
	}
}
