"""Writes dist/Data/Interface/Translations/IHarvestNG_ENGLISH.txt from the string table in src/core/src/Text.cpp.

Run after adding or changing a string: python -I tools/make_translation.py
The file is UTF-16 LE with BOM and CRLF line ends, the format SKSE and the game expect. A unit test
(tests/TextTests.cpp) fails when the file and the table disagree.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "src/core/src/Text.cpp"
OUT = ROOT / "dist/Data/Interface/Translations/IHarvestNG_ENGLISH.txt"

ENTRY = re.compile(r'Entry\{\s*"(\$IHNG_[A-Za-z0-9_]+)",\s*"((?:[^"\\]|\\.)*)"\s*\}')


def main() -> int:
	entries = ENTRY.findall(SOURCE.read_text(encoding="utf-8"))
	if not entries:
		raise SystemExit(f"no Entry{{...}} found in {SOURCE}")
	keys = [k for k, _ in entries]
	if len(keys) != len(set(keys)):
		raise SystemExit("duplicate keys in the string table")
	for key, text in entries:
		if "\\" in text or "\t" in text:
			raise SystemExit(f"{key}: escapes and tabs are not supported in translation text")
	body = "".join(f"{key}\t{text}\r\n" for key, text in entries)
	OUT.parent.mkdir(parents=True, exist_ok=True)
	OUT.write_bytes(b"\xff\xfe" + body.encode("utf-16-le"))
	print(f"{len(entries)} strings -> {OUT.relative_to(ROOT)}")
	return 0


if __name__ == "__main__":
	sys.exit(main())
