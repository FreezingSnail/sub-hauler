# Native stack-probe runner

`runner.cpp` loads a noUSB game HEX plus the real `fxdata/fxdata.bin` into the
pinned Ardens `absim` core, advances a fixed number of milliseconds, and prints
the bytes of an SRAM diagnostic symbol. It is a measurement tool only; it is
never linked into the game.

The .11.2 evidence used a build-only staged full-game image with:

- `ARDUBOY_NO_USB` immediately before `setup()`;
- `SH_AUDIO_PLAYER=1` and the real player;
- the same shipping flags as `make build`;
- a painted stack (`0xA5` from `__bss_end + 1` to `SP - 64`), then cold/cached
  world frames, audio service, pause/resume/mute and Timer3 ISR activity;
- a 20-byte `stack_diag` record.

The resulting image is `build/audio-admission/stack/dist/stack.ino.elf`; the
runner log is `build/audio-admission/stack/run.log`. The measured result was
`reserve=815` against the 600 B target.

Rebuild the runner against the already-built absim objects:

```sh
ARDENS=/Users/connorfranc/code/arduboy/Ardens
c++ -std=c++17 -O2 -DNDEBUG -DARDENS_NO_DEBUGGER -DARDENS_NO_SNAPSHOTS \
  -DARDENS_VERSION_MAJOR=0 -DARDENS_VERSION_MINOR=0 -DARDENS_VERSION_PATCH=0 \
  -DMINIZ_STATIC_DEFINE -I "$ARDENS/src" -I "$ARDENS/deps" \
  -I "$ARDENS/deps/bitsery/include" -I "$ARDENS/deps/miniz" \
  -I build/audio-reference/native/include \
  -c tools/audio_stack/runner.cpp -o build/audio-stack/runner.o
objs=(build/audio-reference/native/obj/*.o); objs=(${objs:#*capture.o})
c++ -O2 build/audio-stack/runner.o "${objs[@]}" -o build/audio-stack/runner -lm
```

Run it with the SRAM address from `avr-nm` (subtract `0x800000`):

```sh
./build/audio-stack/runner --hex stack.ino.hex --fx fxdata/fxdata.bin \
  --ms 3000 --diag-addr 0x112 --diag-bytes 20
```
