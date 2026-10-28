#!/bin/zsh
# Sampled IMA ADPCM + Timer3/Timer4 PWM side of the emulator comparison.
# Self-contained: uses the firmware and complete cart beside this script; never
# rebuilds or reads the repository working directory.
set -euo pipefail

HERE="$(cd -- "$(dirname -- "$0")" && pwd -P)"
DEFAULT_ARDENS="/Users/connorfranc/code/arduboy/Ardens/build/Ardens.app/Contents/MacOS/Ardens"
ARDENS="${ARDENS:-$DEFAULT_ARDENS}"
if [[ ! -x "$ARDENS" && -x "$DEFAULT_ARDENS" ]]; then
  ARDENS="$DEFAULT_ARDENS"
fi
ELF="$HERE/firmware/sampled.ino.elf"
CART="$HERE/cart/fxdata.bin"

if [[ ! -x "$ARDENS" ]]; then
  echo "Ardens not found at: $ARDENS" >&2
  echo "Set ARDENS=/absolute/path/to/Ardens and run again." >&2
  exit 1
fi
if [[ ! -f "$ELF" || ! -f "$CART" ]]; then
  echo "Missing firmware or complete cart in $HERE" >&2
  exit 1
fi

exec "$ARDENS" volume=100 display=ssd1306 fxport=d1 file="$ELF" file="$CART"
