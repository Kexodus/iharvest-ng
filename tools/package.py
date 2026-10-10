#!/usr/bin/env python3
"""package.py - build the mod-manager-installable release archive.

    python -I tools/package.py [--build build/plugin] [--out release]

Produces release/IHarvestNG-<version>.zip with Data/ contents at the archive root:
IHarvestNG.esp, SKSE/Plugins/IHarvestNG.dll + .ini, meshes/, textures/, sound/.
"""

from __future__ import annotations

import argparse
import re
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def project_version() -> str:
	text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
	m = re.search(r"project\(\s*IHarvestNG\s+VERSION\s+([0-9.]+)", text)
	if not m:
		raise SystemExit("could not read the project version from CMakeLists.txt")
	return m.group(1)


def main() -> int:
	ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
	ap.add_argument("--build", type=Path, default=ROOT / "build/plugin", help="CMake build dir of the plugin preset")
	ap.add_argument("--out", type=Path, default=ROOT / "release")
	args = ap.parse_args()

	dll = args.build / "src/plugin/Release/IHarvestNG.dll"
	if not dll.exists():
		raise SystemExit(f"{dll} not found - build the plugin-release preset first")
	dist = ROOT / "dist/Data"

	version = project_version()
	args.out.mkdir(parents=True, exist_ok=True)
	archive = args.out / f"IHarvestNG-{version}.zip"
	with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
		for path in sorted(dist.rglob("*")):
			if path.is_file():
				z.write(path, path.relative_to(dist).as_posix())
		z.write(dll, "SKSE/Plugins/IHarvestNG.dll")
		# GPL-3.0 wants the license text shipped with the DLL; keep it next to it, out of the archive root.
		z.write(ROOT / "LICENSE", "SKSE/Plugins/IHarvestNG-LICENSE.txt")
	with zipfile.ZipFile(archive) as z:
		for info in z.infolist():
			print(f"{info.file_size:>10}  {info.filename}")
	print(f"-> {archive} ({archive.stat().st_size} bytes)")
	return 0


if __name__ == "__main__":
	sys.exit(main())
