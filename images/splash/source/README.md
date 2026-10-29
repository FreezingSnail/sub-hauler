# Splash sources

`snailware_64x64.png` is the original, unmodified 64x64 Snailware logo from
[FreezingSnail/AllYourArdu](https://github.com/FreezingSnail/AllYourArdu/blob/main/sprites/snailware_64x64.png).
Its upstream Git blob SHA is `041a6158456bafbfe97f00d219fce107b6e5512b`;
SHA-256 is `ce552979ff046c7fd623009dceab6d79cfde13cf9e37d170aa4a8221e9a0a2fb`.
The upstream MIT license (Copyright 2023 Freezing Snail) is retained here.

`make gen` runs `tools/gen_splash.py` to place the native logo at (32,0), with
SNAIL and WARE beside it as in AllYourArdu. The second frame composes the
existing HAULER silhouette and project font with an underwater title scene.
Both frames are opaque 128x64, white on black; no new runtime font or art
renderer is needed. The generated preview is `build/splash-preview.png`.
