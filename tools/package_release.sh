#!/usr/bin/env bash
# Build the two release packages:
#   sub-hauler-fx.arduboy    Arduboy FX     (HEX + FX cart image + save area + screenshot + info.json)
#   sub-hauler-mini.arduboy  Arduboy Mini   (HEX + FX cart image + save area + screenshot + info.json)
#
# Usage: tools/package_release.sh <version> [output-dir]
# The version may include a leading v. Inputs must already be built:
#   make build mini release-package
set -euo pipefail
export TZ=UTC

cd "$(dirname "$0")/.."

version="${1:?usage: tools/package_release.sh <version> [output-dir]}"
version="${version#v}"
out_dir="${2:-dist/release}"

fx_hex="dist/fx/sub-hauler.ino.hex"
mini_hex="dist/mini/sub-hauler.ino.hex"
fxdata="fxdata/fxdata.bin"
fxsave="fxdata/fxdata-save.bin"
splash_sheet="images/splash/startup_128x64.png"
stage="build/release-package"
release_date="$(git show -s --format=%cs HEAD)"

for file in "$fx_hex" "$mini_hex" "$fxdata" "$fxsave" "$splash_sheet"; do
	test -s "$file" || { echo "package-release: missing or empty $file" >&2; exit 1; }
done

# The save file is regenerated alongside the cart image, but gen-check only
# compares fxdata.bin: verify the save area really is the image tail.
test "$(tail -c 4096 "$fxdata" | wc -c)" -eq 4096
tail -c 4096 "$fxdata" | cmp -s - "$fxsave" || {
	echo "package-release: $fxsave is not the 4096-byte tail of $fxdata" >&2
	exit 1
}

rm -rf "$stage"
mkdir -p "$stage" "$out_dir"

# The .arduboy screenshot is the game's own startup frame: the second 128x64
# pose of the generated splash sheet (title over the first-arrival art). Save a
# strict 1-bit black/white PNG as the spec requires.
python3 - "$splash_sheet" "$stage/screenshot.png" <<'PY'
import sys
from PIL import Image

sheet_path, out_path = sys.argv[1:]
sheet = Image.open(sheet_path).convert("L")
frame = sheet.crop((128, 0, 256, 64))
frame.point(lambda p: 255 if p >= 128 else 0).convert("1").save(out_path)
PY

make_package() {
	local label="$1"
	local title="$2"
	local device="$3"
	local hex="$4"
	local hex_name="$5"
	local output="$6"
	local package_stage="$stage/$label"
	local output_abs

	rm -rf "$package_stage"
	mkdir -p "$package_stage"
	cp "$hex" "$package_stage/$hex_name"
	cp "$fxdata" "$package_stage/fxdata.bin"
	cp "$fxsave" "$package_stage/fx-save.bin"
	cp "$stage/screenshot.png" "$package_stage/screenshot.png"

	python3 - "$package_stage/info.json" "$version" "$release_date" "$title" "$device" "$hex_name" <<'PY'
import json
import sys

path, version, release_date, title, device, hex_name = sys.argv[1:]
info = {
    "schemaVersion": 3,
    "title": "Sub-hauler",
    "description": (
        "A compact underwater salvage campaign. "
        "Recover the cutter, pressure seal, current thruster and survey lens, "
        "then reach the DEEP RELAY."
    ),
    "author": "FreezingSnail",
    "version": version,
    "date": release_date,
    "genre": "Action",
    "publisher": "Snailware",
    "url": "https://github.com/FreezingSnail/sub-hauler",
    "sourceUrl": "https://github.com/FreezingSnail/sub-hauler",
    "screenshots": [
        {
            "title": "Title screen",
            "filename": "screenshot.png",
        }
    ],
    "binaries": [
        {
            "title": title,
            "filename": hex_name,
            "device": device,
            "flashdata": "fxdata.bin",
            "flashsave": "fx-save.bin",
        }
    ],
    "buttons": [
        {"control": "Left / Right", "action": "Thrust"},
        {"control": "Up / Down", "action": "Blow or flood ballast"},
        {"control": "A", "action": "Confirm; hold with the thruster to boost"},
        {"control": "B", "action": "Open the chart; back out of menus"},
        {"control": "A + B", "action": "Pause"},
    ],
}
with open(path, "w", encoding="utf-8", newline="\n") as info_file:
    json.dump(info, info_file, indent=2, ensure_ascii=False)
    info_file.write("\n")
PY

	# Keep archives byte-stable: fixed member order, normalized mtimes, no
	# extra attributes. Each zip contains exactly the files named by info.json
	# (screenshot.png is named by the top-level screenshots entry).
	touch -t 200101010000 \
		"$package_stage/info.json" "$package_stage/$hex_name" "$package_stage/fxdata.bin" \
		"$package_stage/fx-save.bin" "$package_stage/screenshot.png"
	output_abs="$(cd "$(dirname "$output")" && pwd)/$(basename "$output")"
	rm -f "$output"
	(
		cd "$package_stage"
		zip -X -q -9 "$output_abs" info.json "$hex_name" fxdata.bin fx-save.bin screenshot.png
	)
	unzip -tq "$output"
}

make_package \
	fx "Sub-hauler (Arduboy FX)" ArduboyFX \
	"$fx_hex" sub-hauler-fx.hex "$out_dir/sub-hauler-fx.arduboy"
make_package \
	mini "Sub-hauler (Arduboy Mini)" ArduboyMini \
	"$mini_hex" sub-hauler-mini.hex "$out_dir/sub-hauler-mini.arduboy"

printf 'package-release: %s %s\n' \
	"$out_dir/sub-hauler-fx.arduboy" "$out_dir/sub-hauler-mini.arduboy"
