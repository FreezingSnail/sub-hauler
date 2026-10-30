# Story resource measurements — 2026-10-27

> **Historical record — archived 2026-10-30.** Dated measurements, not release acceptance. Current limits live in [platform.md](../platform.md).

All comparison variants started at `f340fb534e11fc95a1b3f7b977fc8317f42d10cc`
plus the same in-progress campaign snapshot. Its tracked binary patch SHA-256
is `db6aea01f5ff6dbf65500a1ceaa500bb88e465700482214fc94958c53f276ea1`.
These are dated measurements, not release acceptance.

| Full profile | Flash | Static RAM | Memory-probe reserve |
| --- | ---: | ---: | ---: |
| Native campaign + save, no music | 30,432 B | 1,663 B | 711 B |
| FX condition table + save, no music | 30,566 B | 1,663 B | 711 B |
| Story VM + save, no music | 30,592 B | 1,721 B | 660 B |
| Native + save, USB-free build probe | 27,764 B | 1,525 B | unmeasured |
| USB-free probe + live music player/refill | 31,584 B | 1,753 B | unmeasured |

Native/table/VM campaign-only flash/static were respectively
25,676/1,639 B, 25,846/1,639 B and 25,876/1,695 B. Native/table/VM host tests
passed 201/208/206 suites with campaign and saving; tooling passed 109/115/109.
All three generation checks were deterministic. The stack values are from the
same expanded Ardens test_memory probe, not complete-game guarantees.

The table adds 188 B of FX records, has no cache, and performs 51–62 reads in
the measured WORK drawing cases. Idle menus skip redraws. The VM adds 218 B
of FX bytecode; its read count and input latency were not measured. Neither
condition representation reduced the whole firmware image for this story.

The supported Arduboy2 USB-free entry point saved 2,668 B flash and 138 B
static in the build-only probe. Adding the real ATMlib2FX player, one cue and
foreground refill then cost 3,820/228 B; region/mute/save lifecycle controls
were not implemented in that probe. No physical sound or loader-entry test
was claimed.

## Archived evidence

The cleanup retained `build/research/story-prototypes-20261027/evidence.tar.gz`
(36,036,431 B), SHA-256
`d215567f91748097ac1c0de408e6ddfda9bfed44693c6995baff6fcad63d25f2`.
Its `MANIFEST.json` verifies 4,432 saved files individually. It includes binary
source patches, untracked prerequisites, reports, selected ELFs and the
build-only probe sketches/logs. Reconstruct a variant from the recorded base
revision, its `changes.patch` and its `untracked/` files. The aggregate report
is `comparison/REPORT.md` inside the archive. Native and table worker reports
are also retained beside the archive.

Superseded comparison worktrees and owned size-audit scratch were removed
after archive verification. Compiler cache cleanup plus worktree removal
reclaimed about 660 MiB, including creation of the new isolated audio-admission
worktree. The main worktree's campaign source patch was verified unchanged.
