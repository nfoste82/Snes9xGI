# Live application lighting benchmark

Run from an unlocked macOS desktop session. The runner opens the specified ROM,
loads its matching profile, restores the save state on the emulator thread before
each case, and ignores physical controller/keyboard input during the benchmark.
It exits automatically with status 0 on completion, 2 on app-side failure.

```sh
python3 tools/run_remaster_benchmark.py \
  --app "$HOME/Library/Developer/Xcode/DerivedData/snes9x-atcnlahonwpbthgwbglgovfsrdlq/Build/Products/Release/Snes9x.app" \
  --output build/benchmark-slot004-right \
  --move right --seconds 3
```

Defaults use `~/Downloads/Zelda_ALTTP.zip`,
`~/Library/Application Support/Snes9x/Freezes/Zelda_ALTTP.004.frz`, and the
repository's `alttp-profile.toml`. Override with `--rom`, `--state`, `--profile`.
The output directory must be new. `--connections` overrides the profile setting.

Cases: Direct lighting only; Direct + Indirect at 0/1/8 bounces; Composite at
0/1/8 bounces. The profile supplies other lighting settings. The debug sphere is
disabled. Audio output is muted automatically, including after each save-state
restore. GPU diagnostic/readback environment variables are disabled by the runner.

`--move none|left|right` holds the direction for the measured emulated frames;
warmup (default 0.5 emulated seconds) has no input. Each case restores the state
and resets the shader random seed. End-state RAM hashes verify case consistency.
Whether movement actually scrolls depends on the selected save state's location;
the script cannot force scrolling through an obstructing wall.

`--presentation deterministic` (default) renders every emulated frame and waits
for completion: identical animation/movement frames are compared across cases.
Three emulated seconds means 180 measured frames at 60 Hz, and can take minutes
of wall time when rendering is slow. `--presentation live` keeps normal bounded
asynchronous presentation and frame dropping; its submitted frame subset depends
on performance. This mode is useful for real gameplay throughput, but not
pixel-exact comparisons. Both force frame skip 1 and disable sound synchronization.

`report.json` contains input hashes, executable/build identity, hardware, settings,
per-frame GPU and matched field-preparation timings, throughput, dropped counts,
and median/p95/p99 summaries. Emulator refresh/pacing values are explicitly latest
samples and can refer to a previous frame. Frames/s is GPU-completion throughput,
not a display-present callback measurement. GPU duration includes all command
buffer graphics, not isolated direct/indirect passes. PNGs and `.s9xrmf` captures
are written after each case outside its timed interval. Reports are checkpointed
after each case so completed results survive interruption. Benchmark shutdown
does not save benchmark-driven SRAM.

## Verified baseline (October 1, 2026)

M3 Max, Release build, slot 004, repository profile (96 connections), right held
for 180 measured frames after 30 warmup frames. All seven cases had 180 completed
samples and matching final RAM hashes. The final image confirms Link moved to
the right and the scene scrolled. GPU median/p95 milliseconds:

| Case | Median | p95 |
| --- | ---: | ---: |
| Direct only | 230.03 | 477.31 |
| Direct + Indirect, 0 | 229.46 | 475.98 |
| Direct + Indirect, 1 | 401.73 | 693.50 |
| Direct + Indirect, 8 | 448.10 | 821.98 |
| Composite, 0 | 225.67 | 475.47 |
| Composite, 1 | 270.53 | 534.23 |
| Composite, 8 | 422.80 | 652.28 |

Scene-field medians were 4.84–6.94 ms; mesh medians 2.92–4.09 ms.
These are whole-command-buffer timings from serial deterministic presentation,
not isolated pass timings. The direct+indirect and composite one-bounce variation
needs repeated profiling before interpreting it as a view-specific cost.
The complete run took roughly ten wall-clock minutes. A separate live-mode smoke
run completed all seven cases with normal dropped frames and matching RAM hashes.

### Direct-shader specialization

The production direct pass uses a Metal function constant to compile out radial
indirect quadrature and diagnostic branches. Visibility/diagnostic/reference
passes retain the unspecialized variant and existing triangle intersection path.
Analytic flat-patch and early-triangle-rejection experiments were removed after
live measurements regressed; neither changes the final geometry path.

The same muted, right-moving, 180-frame/96-connection run then measured:

| Case | GPU median | GPU p95 |
| --- | ---: | ---: |
| Direct only | 84.81 | 168.93 |
| Direct + Indirect, 0 | 83.59 | 169.72 |
| Direct + Indirect, 1 | 125.37 | 207.61 |
| Direct + Indirect, 8 | 252.33 | 343.69 |
| Composite, 0 | 84.36 | 170.40 |
| Composite, 1 | 126.88 | 210.16 |
| Composite, 8 | 256.19 | 344.61 |

All cases completed 180 samples with matching RAM hashes. The lighting suite
passed 1215/1215 checks. Synthetic full-frame direct comparisons differed in
5–7 half-float channels per scene (of 229,376); every difference was within one
half-float step, maximum relative error below 0.00086. Specialization can change
compiler rounding, so direct comparisons enforce that explicit tolerance rather
than bit identity. Synthetic open-scene timings did not consistently improve;
this optimization is validated against the authored slot-004 workload, not a
claim of universal speedup. Earlier/later live results also include machine load
and thermal variability and are not simultaneous A/B measurements.

The app also accepts these options directly:
`--remaster-benchmark --benchmark-rom PATH --benchmark-state PATH
--benchmark-profile PATH --benchmark-output PATH --benchmark-seconds 3
--benchmark-warmup-seconds 0.5 --benchmark-move right
--benchmark-presentation deterministic --benchmark-connections 8`.

## Session handoff: resume GPU optimization

Read `AIDocs/00-index.md` first, then this note and `remaster/lighting.md`.
The user wants GPU frame time addressed before CPU scene fields. Benchmark mode
and direct-source preparation/specialization are implemented; CPU field caching,
buffer reuse, and material-resolution optimization have not yet been implemented.
Changes from this work are currently uncommitted; inspect `git status` and diff
before continuing, and preserve the current working tree.

Relevant code:
- `tools/run_remaster_benchmark.py`: launch/setup and report summary.
- `macosx/mac-os.mm`: benchmark state machine, save-state reset, input sequence,
  report output, and automatic audio mute.
- `macosx/remaster-benchmark.h`: producing-frame tags and completion sample ABI.
- `macosx/mac-render.mm`: GPU source preparation, specialized pipeline creation,
  frame-tagged timing collection, draining, screenshots, and mesh/wait metrics.
- `macosx/shaders.metal`: source preparation and direct function constant;
  `remasterVisibility` and `remasterMeshHit` are the next profiling targets.
- `macosx/remaster-lighting-test.mm`: production shader regression suite.
- `macosx/remaster-lighting-benchmark.mm`: synthetic direct comparison (`--direct`).

Next steps:
1. Obtain isolated GPU stage timings or counters for the captured authored room.
   Current live reporting is whole-command-buffer GPU duration. Verify traversal,
   triangle intersection, and emitter enumeration costs before another algorithm
   change. Direct currently enumerates every emissive sample for every receiver;
   zero indirect bounces does not avoid direct transport.
2. Reduce receiver × emitter visibility work, preserving fractional opacity,
   geometric endpoints, emission-depth energy, and current mesh semantics. Consider
   tighter conservative acceleration or source reuse; do not assume analytic flat
   patch paths are faster (the attempted shortcuts regressed and were removed).
3. Compare identical slot-004 movement sequences and captured outputs; retain
   reference visibility comparisons and the documented half-float tolerance.
4. Address scene fields after the GPU work: distinguish waits from actual work,
   reuse scratch storage, pre-resolve materials, remove redundant gradient normal
   work, then consider geometry caching with correct movement/edit invalidation.

No room-height metadata was generated or approved in this performance work. If
future changes involve ALTTP room-height/floor/wall placement generation or
validation, follow the repository's required room-height-expert delegation.

Validation commands (temporary parent already exists):

```sh
xcrun clang++ -std=c++17 -O2 -fobjc-arc -Wall -Wextra \
  macosx/remaster-lighting-test.mm -framework Foundation -framework Metal \
  -o /var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/visibility-fast-test
/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/visibility-fast-test
xcrun clang++ -std=c++17 -O2 -fobjc-arc -Wall -Wextra \
  macosx/remaster-lighting-benchmark.mm -framework Foundation -framework Metal \
  -o /var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/visibility-specialized-benchmark
/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/visibility-specialized-benchmark --direct
xcodebuild -quiet -project macosx/snes9x.xcodeproj -scheme snes9x \
  -configuration Release -destination "platform=macOS,arch=arm64" \
  CODE_SIGNING_ALLOWED=NO build
git diff --check
```

Raw reports/images/captures from this session are local temporary artifacts:
- Baseline: `/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/live-benchmark-slot004-right-3s/`
- Final specialization: `/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/live-benchmark-direct-specialization-only-3s/`
- Rejected visibility experiments: `/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/live-benchmark-gpu-specialized-right-3s/`

Temporary artifacts may be cleared by macOS; the tables and reproduction
instructions above are the durable record. `AIDocs/` is intentionally local-only
and ignored, while this note and the benchmark tools can be tracked normally.
