# Remaster correctness gate

From the repository root, after a fix:

```sh
python3 tools/run_remaster_tests.py
```

Requires Python 3.9+ (standard library only) and a C++17 compiler (`CXX` or
`c++`). The runner resolves the repository from its own location, so invocation
from another working directory also works. No ROM, emulator, save state, game,
generated room bundle or user-specific capture path is needed. The pthread
stack cases in `full` also load the checked-in
`Profiles/ALttP/alttp-profile.toml`, whose exact bytes are fingerprinted.

## Commands

```sh
# Inspect the curated selection; no compiler/GPU or output directory needed.
python3 tools/run_remaster_tests.py --list
python3 tools/run_remaster_tests.py --suite full --metal --list

# Ten essential lifecycle/ownership/height/source regressions (default).
python3 tools/run_remaster_tests.py --suite quick

# Eighteen cases, including placement/semantic reuse, terrain and stack safety.
python3 tools/run_remaster_tests.py --suite full

# ASan + UBSan; findings terminate the test and fail the aggregate.
python3 tools/run_remaster_tests.py --sanitize

# Append production shader regressions, after all CPU tests finish.
python3 tools/run_remaster_tests.py --metal

# Retain independent results, or choose a compiler/timeout explicitly.
python3 tools/run_remaster_tests.py --suite full --output build/remaster-tests-review
python3 tools/run_remaster_tests.py --cxx clang++ --timeout 240

# Python runner contracts; no C++ compiler or GPU required.
python3 -m unittest discover -s tools -p test_run_remaster_tests.py
```

`--metal` requires macOS, the Foundation/Metal SDK, and an accessible Metal
device. Missing platform, compiler/framework, device, or runtime shader
compilation is **failure**, never a skip or a pass. The shader suite loads
`macosx/shaders.metal` through the real Metal API. `--sanitize --metal` also
instruments the CPU Objective-C++ test harness; it does not instrument GPU
shader memory. Do not overlap this suite with a live/GPU benchmark. The runner
does not launch any benchmark, emulator, or parallel test workers.

## What runs

| Suite | Test | Production-path contract |
|---|---|---|
| quick | `dungeon_departure_geometry` | Synthetic `EndFrame`: settled/departing geometry, both BGs and pixel-local ramps; partial construction/plane changes, producer/word rejection, retirement, reset/defrost, ROM/profile change |
| quick | `dungeon_transition_cache` | `EndFrame`: construction/collision completion gate, mixed-room source retention, actor plane, fallback ramp, cache epochs |
| quick | `dungeon_geometry_cache` | `EndFrame`: animation reuse versus forced full solve; complete two-BG maps/diagnostics and serialized packet equivalence; producer refresh and geometry dependency invalidation |
| quick | `dungeon_entry_lifecycle` | `EndFrame`: observed door continuity from signed active-plane support; both BGs, stale datum expiration, room jumps/airborne/warp/fall/stair rejection |
| quick | `dungeon_scroll_ownership` | Four directions, loader origins and 16-bit wrap; actual split-instance bases, upload-word and transformed producer rejection |
| quick | `dungeon_background_ownership` | Independent BG/plane structural seams; authored H/V geometry, effective runtime offsets, unresolved-support rejection |
| quick | `dungeon_mixed_floor` | Effective placement base + local foot/ramp composition across flips; preserve authored slope/normals; reject unknown/ambiguous plane ownership |
| quick | `dungeon_disconnected_plane` | Overlapping/disjoint unobserved planes remain explicit; distinct BG graphics bases and complete-input key dependencies |
| quick | `offscreen_continuity` | Production exact-depth source generation and hidden sampling: partial/offscreen area and power, signed positions/HES2 roundtrip, flips/normals, deterministic representative clusters, HUD versus world coverage |
| quick | `held_lamp` | Lamp state/facing/anchor; actual HUD/dialogue/UI ink, transparent HUD and world sprites; invalid-input rejection |
| full adds | `dungeon_placement_reuse` | `EndFrame`: retained placement relations, irrelevant VRAM/actor reuse and complete map/plane/profile invalidation |
| full adds | `dungeon_semantic_cache` | `EndFrame`: semantic activation/facing/context/door-mask rebuild versus exact reload reuse |
| full adds | `dungeon_terrain_lifecycle` | Pickup/break support, carried/thrown/landed object plane/Z, OAM transforms and ambiguous ownership rejection |
| full adds | `dungeon_persistent_support_rejection` | Dual-owner footprint remains unresolved without merging components or corrupting constraints |
| full adds | `emitter_patches` | Production visible source clustering: exact member coverage, holes, depth/direction boundaries, reciprocal animation links and separated repeated placements |
| full adds | `sphere_samples` | Production distance-based sample budgets and invalid-input defaults |
| full adds | `dungeon_small_stack_debug`, `dungeon_small_stack_optimized` | Solver, both application paths and `EndFrame`/cache on 512/544 KiB pthread stacks; checked-in profile; assertions/checks active in both builds |
| opt-in | `metal_lighting` | Synthetic production GPU direct/indirect transport, depth, sampling, hidden sources, patches, visibility and reference comparisons |

The selection is explicit, not a glob of all `*_test.cpp`. Some synthetic
application cases inject already-solved bases to isolate ownership/transport;
they do not prove that those bases were correctly inferred in gameplay.

## Output and failure semantics

Each invocation creates a unique directory under `--output` (default
`build/remaster-tests`), prints its `results.json` path, and preserves prior
runs. Inside it:

- `compiler.log`: compiler version output, including errors.
- `<test>/compile.log`, `<test>/run.log`: exact argv and complete combined
  stdout/stderr. Warnings are retained, not suppressed. Setup errors have a
  `runner.log`; failed compilations never execute a stale binary.
- `<test>/test`: freshly compiled executable; no incremental binary reuse.
- `results.json`: selection, pass/fail/not-executed counts, commands, process
  exit codes, per-phase seconds, compiler version/executable SHA-256, binary
  SHA-256, Git HEAD/dirty status, source SHA-256 manifests and run environment.

Source manifests conservatively include the selected translation unit, runner,
all `remaster/**/*.h` (including dirty/untracked headers), explicit profile
inputs, and shaders when selected. This is a local-source dependency superset,
not a fingerprint of SDK/system headers or the entire compiler toolchain. The
runner checks inputs against suite-start, after each case, and at suite-end;
concurrent edits invalidate the gate. Avoid editing covered inputs during a run.

All selected tests run sequentially even after ordinary failures. A compile
failure, assertion/signal/nonzero exit, timeout, unavailable requested Metal,
source mutation or setup failure makes the aggregate exit **1**. Invalid CLI
usage exits **2**. Interruptions retain a partial JSON report with remaining
selected tests explicitly not executed. Exit **0** requires every selected
case to have compiled and returned zero; `--list` only lists, not validates.

Compile flags follow the direct-test commands: `-std=c++17 -Wall -Wextra
-pedantic -O0 -g -UNDEBUG`; stack cases add `-pthread`, and the optimized case
appends `-O2` without disabling assertions. Metal adds `-fobjc-arc -framework
Foundation -framework Metal`. Sanitizing adds `-fsanitize=address,undefined
-fno-omit-frame-pointer -fno-sanitize-recover=all` and fatal runtime options.
Optional fixture/shader-variant environment switches are removed and recorded
so inherited shell settings cannot silently change the synthetic selection.

Wall-clock seconds and timeouts help detect runaway/hanging tests; they are
**not performance thresholds** or evidence of frame/GPU budget compliance.
Performance measurement/comparison belongs to the separate benchmark tooling.

## Known legacy fixture and coverage gaps

`remaster/remaster_test.cpp` is deliberately excluded from both default suites.
To investigate it explicitly:

```sh
python3 tools/run_remaster_tests.py --legacy-smoke
```

On this worktree it compiles but aborts at line 739:
`S9xDeserializeRemasterFrame(previousVersionBytes, previousVersionFrame)`.
The fixture manufactures v16 bytes by deleting hard-coded slices from a current
serialized packet (lines 726–739); it is not a pinned historical v16 packet.
Its surgery has fallen behind the current format. The earlier hash-vector and
other checks reached before this assertion do not turn the failed executable
into a pass. This opt-in is a normal failure, not an expected-failure allowance.
No production parser or fixture assertion is weakened here. A safe follow-up
is a reviewed, independently constructed/pinned historical packet regression,
not arbitrary offset adjustment merely to obtain a green run.

Not covered by this gate:

- No captured transition corpus, live gameplay/camera comparison, ROM dispatcher
  execution, GUI reset/defrost workflow or complete state-space exploration.
  Synthetic `EndFrame` checks runtime policy, not every actual WRAM sequence.
- No whole-room approval, corpus-wide tile/plane denominator audit or room-wide
  validator. Passing this runner never means room heights are complete/correct.
- Capture-dependent `hud_emitter_capture_test`, placement-equivalence and
  exact-capture height tests require separately pinned assets; they are not
  run without arguments and then misreported as useful coverage.
- `capture_path_test` currently embeds a host-specific temporary directory and
  is excluded rather than depending on that machine path.
- `offscreen_emitters_test` and `offscreen_emitter_bases_test` explicitly enable
  `ALTTP_TEST_LEGACY_OFFSCREEN_SPHERES`; those legacy approximation oracles are
  not counted as production exact-depth coverage. `offscreen_continuity` covers
  production source continuity/HUD/hidden clustering but is not a substitute
  for all legacy fixture assertions or captured HUD replay evidence.
- This runner does not build the macOS app or verify all renderer modes.

## Observed validation

On Apple M3 Max, Apple clang 17 (arm64 macOS), October 2026:

- Full portable: **18/18**, 52.92 s (before final runner-only reporting hardening).
- Final quick portable: **10/10**, 27.94 s; Python runner contracts **6/6**.
- Quick ASan/UBSan: **10/10**, 57.88 s.
- Quick + Metal: **11/11**, 38.63 s; Metal **1377/1377 checks**, zero failures.
- Explicit legacy investigation: **10/11**, nonzero aggregate, 31.17 s;
  the documented line-739 legacy fixture assertion reproduced.

These are correctness-run elapsed times including compilation, not benchmarks.
Per-run JSON/logs remain under `build/remaster-tests`; rerun after new fixes.
# Performance and gameplay fixtures

See [remaster-regressions.md](remaster-regressions.md) for freeze-based Release
benchmarks, repeated baseline recording, performance failure thresholds and
the distinction between synthetic transition checks and gameplay captures.
