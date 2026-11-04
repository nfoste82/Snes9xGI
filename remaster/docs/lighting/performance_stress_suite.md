# ALttP performance stress suite

`remaster/tools/remaster_stress.py` is an opt-in Release-app regression gate,
separate from ROM-free correctness checks. It builds on the existing app
benchmark rather than creating another rendering implementation. It restores
local freezes and feeds normal joypad input; it never writes gameplay RAM to
manufacture movement/transitions.

## Scenario matrix

Each JSON scenario pins a ROM file, freeze and **private copied profile**, defines
a measured-frame route and sweeps visible representatives, offscreen
representatives and deterministic/live presentation separately. Explicit private
representative overrides include per-area settings so room inheritance cannot
silently defeat the sweep. Authored files are not rewritten.

Supported route buttons: `none`, four directions, `a/b/x/y`; chords such as
`["up", "a"]` use the normal controller path. Steps have integer frame counts;
warm-up is stationary. Routes and exact measured frames must agree. Useful
fixtures include dense room012 lights, a populated movable-obstacle room, a
cross-room route, a doorway and a straight-stair transition. Further scenarios
can combine categories or script pickup/throw inputs; an input script alone is
not proof that a pickup actually succeeded.

The example `remaster/tools/ALttP/performance-stress.example.json` is a **template,
not an acquired fixture**. No ROM/save is included and no listed route is claimed
to work until acquired and validated. Exact representative setting 0 (no
approximation) is supported but can be extremely expensive; add it deliberately
as a separate slow tier rather than make every developer run it by accident.

## Acquire local fixtures once

1. Save freezes from the desired gameplay positions yourself, with Link facing
   the required direction, an unobstructed movement path, and the correct door
   or stairs nearby. Room transitions need source and destination room IDs.
2. Copy the matching complete profile into the local fixture directory; preserve
   its authored movable groups. Do not toggle groups in the canonical profile
   merely to make a test pass. Choose a room with actual classified placements
   of those groups. Include before-pickup/after-pickup freezes as separate cases
   if desired.
3. Copy/edit the example manifest under ignored `build/` or another local path.
   Relative fixture paths resolve relative to that manifest, not this repository.
4. Pin the files into a **new** manifest and inspect the matrix, without launching:

```sh
python3 remaster/tools/remaster_stress.py pin build/stress/scenarios.json \
  --output build/stress/pinned.json
python3 remaster/tools/remaster_stress.py plan build/stress/pinned.json
```

The runner checks hashes before and after every process, including the profile's
adjacent `.geometry-review/*.bin` maps (and their absence). ROMs, freezes, copied
profiles and baseline reports stay local; don't commit or redistribute them.
Changing any fixture/settings requires a deliberate new baseline.

## Record and check

First build the app. Quit any running emulator yourself: the suite **refuses to
launch while Snes9x is running** and never terminates it for you. Avoid other GPU
benchmarks (including optimization agents), heavy applications and profiler
captures. Use the same machine, power mode, desktop/window configuration and OS.
The native benchmark uses a fresh process per workload/repeat, restores the freeze,
mutes benchmark audio and exits; it does not save benchmark-driven SRAM. Its
runner copies the app into its output directory, assigns a unique preference
domain, ad-hoc signs the copy, and uses a fresh `HOME` plus
`CFFIXED_USER_HOME` for Foundation support paths. The normal app and preference
domain are not modified. This isolates ordinary app storage, not a security
sandbox; a dedicated macOS test account remains an additional isolation option.

```sh
python3 remaster/tools/remaster_stress.py record build/stress/pinned.json \
  --app build/release/Snes9x.app \
  --baseline build/stress/baseline.json --output build/stress/baseline-runs
# After a source change and rebuild:
python3 remaster/tools/remaster_stress.py check build/stress/pinned.json \
  --baseline build/stress/baseline.json --output build/stress/candidate-runs
```

Two repetitions per exact workload are the default and minimum (owner request
2026-11-04); higher counts remain opt-in with `--repeats`. The current three-scene,
three-representative, two-presentation matrix therefore takes 36 app runs, not54.
Two repetitions give less noise resistance than three; tolerances are unchanged.
Scenario order rotates, never runs GPU
work in parallel. Complete reports/logs remain after failure. Baselines are never
overwritten automatically; `check` cannot promote a candidate into the baseline.

For CI/developer hooks, run `check` after a Release build on the dedicated macOS
GPU worker and propagate its exit code. Run unit tests in ordinary CI without
private assets:

```sh
python3 -B -m unittest discover -s remaster/tools -p 'test_remaster_stress.py'
```

Exit 0 means comparable evidence without threshold regressions, 1 means measured
regressions, 2 means invalid/missing/changed inputs, failed scenario coverage or
an incomplete run. An unavailable fixture must never become a performance pass.

## What constitutes exercising a scenario

- Many lights: every admitted GPU frame meets the configured minimum prepared
  Direct sample count. A missing light workload cannot be counted as a speedup.
- Movable groups: observed classified placement cells belong to an authored
  group whose `movable` flag is actually true. A mere flag in an unused profile
  group does not qualify. This checks exposure to movable groups, not successful
  player pickup/removal; separate authored actions need additional witnesses.
- Movement: per-emulated-frame Link coordinates span the configured distance
  **within one room**, not just a teleport or an asserted held input.
- Transitions: distinct observed room IDs, an actual door/straight-stair phase,
  and pinned initial/final rooms. Failed/blocked routes are invalid, not passes.
- `stairs_round_trip`: same-room plane changes out and back, staircase submodule
  16, and vertical movement in both directions. `stairs_traversal` requires the
  staircase phase. `indoor_scroll` requires doorway scrolling submodule 1.
- `overworld_exit`: observed indoor-to-outdoor change, exit module 15, and settled
  overworld gameplay module 9. Dungeon room RAM can remain stale after exit and
  is not used as a substitute for outdoor evidence.
- Every measured emulated frame has room/module/Link/plane evidence and the
  expected effective representative counts. Deterministic runs additionally
  require every GPU frame and matching source signatures; live submissions may
  legitimately be a different subset.
- Final RAM/stock color hashes and complete gameplay sequence hashes must match
  between repetitions/builds. Hardware, OS, window size, exact inputs/settings,
  warm-up and script must also match. Build identity may change and executable,
  framework and metallib hashes are recorded. A semantics-changing repair may
  legitimately require a reviewed new baseline, not loosening comparability.

## Metrics and limitations

The gate compares median-of-run GPU, per-frame refresh, field, mesh, setup and
source-construction median/p95 times; transition-frame refresh median/p95/max;
completion throughput; and unsubmitted
fraction. Defaults allow timing increases up to max(15%, 0.5 ms), throughput
decline 15%, unsubmitted fraction increase five percentage points. These are
noise allowances, not statistically guaranteed bounds or a 60 FPS budget. p99
and max remain in artifacts for review. Do not simply raise tolerances to accept
a regression. Repeat with a longer measurement window if noisy.

Refresh evidence is collected on the producer after each emulated frame rather
than attached from latest metrics to a GPU completion. It includes nested fields
and presentation CPU; field timings include mesh and must not be summed again.
GPU and CPU overlap. Live mode currently forces render-every-emulated-frame and
disables sound sync like the existing benchmark; it measures bounded submission
backpressure, **not ordinary automatic-frame-skip/audio pacing**. Completion FPS
is not display-present FPS. Actual drawable presentation callbacks, skipped-frame
pacing and room-transition subphase timing distributions remain separate live-
trace validation; do not label this suite display-cadence coverage.

User-nominated freezes have been acquired as private, read-only, hash-pinned
copies under `build/performance-stress-suite/fixtures-v1/`: `.008` for many lights,
`.009` for movable obstacles/east-west movement/stairs in both directions, and
`.011` for the south-door/stairs/overworld-exit route. Originals may change without
altering these copies. The matching profile snapshot is provisional until
confirmed against the intended gameplay workload. See the local fixture README
and pins; never refresh static inputs in place.

With the user-confirmed canonical profile and permission to run isolated test
processes, all three routes were acquired under
`build/performance-stress-suite/route-validation-v1/`. The movement/stair and
door/stair/overworld routes repeated with exact complete gameplay, final RAM,
stock-pixel and deterministic source-sequence parity. `validated-routes.json`
contains the pinned 18-workload planned matrix (three representative counts and
two presentation modes); route acquisition is not a repeated performance baseline.

Movable follow-up (2026-11-04): the user marked `Liftable_Pot` movable in the
canonical profile. Updated read-only `.009` and profile snapshots are preserved
under `build/performance-stress-suite/fixtures-v2/`, with independent hashes.
The freeze bytes are unchanged; the authored setting is in the new profile.
Runtime validation subsequently confirmed 24 active movable placement cells
throughout the movement/stair route. On 2026-11-04 the full 18-workload matrix
completed twice (36 launches) and recorded the first private baseline at
`build/performance-stress-suite/baseline-v2.json`, using the pinned
`build/performance-stress-suite/stress-v2-pinned.json` manifest. Historical v1
route evidence remains preserved, not silently rebaselined. See
`build/performance-stress-suite/BASELINE-V2-REPORT.md` for execution history and
the exact future `check` command. Independent unchanged-build timing comparison
remains pending; a baseline record alone is not a regression comparison.
Do not call synthetic unit-test reports proof of live performance or all-scenario
coverage. The existing ROM-free correctness gate remains independently required.
