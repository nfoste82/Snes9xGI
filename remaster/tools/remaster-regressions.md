# Remaster regression checks

## Correctness — no ROM, freeze or GUI needed

```sh
python3 remaster/tools/run_remaster_tests.py
python3 remaster/tools/run_remaster_tests.py --suite full
python3 remaster/tools/run_remaster_tests.py --sanitize
python3 remaster/tools/run_remaster_tests.py --metal
```

The quick suite exercises production frame-finalization and transition code,
outgoing geometry retention, reset/edit invalidation and source recovery. Full
adds ownership, cache and source invariants. Metal adds production shader checks
(macOS GPU required). Every requested test must pass; failures return nonzero.
Logs and JSON reports are retained under `build/remaster-tests/`. See
[test details](remaster-tests.md), including the separate known-failing legacy
serialization fixture. These checks do not prove complete room-height coverage.

## Performance — freeze-based, actual Release app

Requires an unlocked macOS desktop, Xcode command-line tools, a Release app,
local ROM and freeze. No manual controller input is needed. Quit other Snes9x
instances before timing; avoid concurrent GPU tests, profiler captures and heavy
GPU applications. The runner launches and exits a fresh process for each repeat.

Build the app before comparing changes:

```sh
xcodebuild -quiet -project macosx/snes9x.xcodeproj -scheme snes9x \
  -configuration Release -destination "platform=macOS,arch=arm64" \
  CODE_SIGNING_ALLOWED=NO build
```

Record a known-good baseline (the parent `build/` exists after building):

```sh
python3 remaster/tools/remaster_performance.py record \
  --baseline build/room061-performance.json --output build/perf-baseline
```

After a change and rebuild, check it against that baseline:

```sh
python3 remaster/tools/remaster_performance.py check \
  --baseline build/room061-performance.json --output build/perf-candidate
```

Defaults: `build/release/Snes9x.app`, `~/Downloads/Zelda_ALTTP.zip`, freeze
`~/Library/Application Support/Snes9x/Freezes/Zelda_ALTTP.006.frz` (known near-torch
workload), tracked ALTTP profile, no movement, Composite/zero bounce, 30 warmup
and approximately 180 measured emulated frames per repeat, three repetitions.
Profile settings supply the rest. Inputs can be overridden with `--app`, `--rom`,
`--state`, `--profile`, `--connections`, `--seconds`, `--warmup-seconds` and
`--repeats`. Each output directory must be new; existing baselines cannot be
overwritten. Use deliberate new baselines when fixtures/settings change.

The gate compares median-of-run GPU, CPU scene-field, mesh and GPU-setup median
and p95 times. It fails if an increase exceeds **both 15% and 0.5 ms** (configurable
with `--relative-percent` / `--absolute-ms`). At least three repetitions are
required. Defaults are noise allowances, not a 60 FPS budget or a statistical
guarantee. Investigate failed runs and repeat under comparable load; do not raise
thresholds simply to accept a regression. Mesh time is included in scene-field
time and must not be added again. Timings do not cover all emulator-thread CPU
work; completion throughput is not display-present FPS.

Reports must have matching GPU/OS, ROM/freeze/profile hashes, settings, overrides,
case list, final emulated RAM, original pixels and deterministic frame tags.
Build identity is recorded but allowed to change. Invalid/incomplete comparisons
return exit 2; measured regressions return 1; passing comparisons return 0.
Experimental/readback environment overrides are removed, stage counters disabled.
remaster/profiles/ROM/freezes remain local; reports and captures go under ignored `build/`.
Lighting pixels can legitimately change after repairs: final PNGs are retained
for review but are not automatically treated as correctness proof by this gate.

`--all-cases` benchmarks Direct, Direct+Indirect and Composite at the existing
0/1/8-bounce cases. This is slower but covers indirect workloads.
`--presentation live` tests asynchronous behavior with frame dropping; keep a
separate live baseline. Live sample subsets can differ and do not support exact
per-frame comparisons. Deterministic mode is the default regression workload.

Previously collected reports can be checked without launching the game:

```sh
python3 remaster/tools/remaster_performance.py compare \
  --baseline build/room061-performance.json --output build/perf-comparison \
  build/perf-candidate/run-01 build/perf-candidate/run-02 build/perf-candidate/run-03
```

## Gameplay transition fixtures

Synthetic transition tests run today, including earliest departure, mixed scroll,
destination solve, retirement and reset/profile/ROM changes. They directly test
effective heights and need no saved game. Captured gameplay transition coverage
is still missing; a passing synthetic test is not a visual playthrough.

For actual scroll performance, save a local freeze with Link just before a
doorway and use `--state PATH --move right` (or `left`) identically for record and
check. Movement is applied for measured emulated frames after stationary warmup;
choose duration/position so Link actually crosses the doorway. The current live
benchmark supports one held horizontal direction, not a multi-step route. Its
final RAM/stock image checks verify identical resulting gameplay, but **do not
assert intermediate outgoing heights**. Use the production-path correctness suite
for that assertion until a recorded transition-sequence replay fixture is added.

Useful fixture set: a static dense-torch view, a HUD-covered torch view, a camera
edge crossing and a doorway transition. Keep each freeze plus its pinned profile
and benchmark baseline together; animation, authored metadata and lighting
settings affect workload. Do not use a baseline recorded with a different profile
as evidence that a code change regressed performance.

## Test the tooling itself

```sh
python3 -m unittest discover -s tools -p 'test_*remaster*.py'
```
