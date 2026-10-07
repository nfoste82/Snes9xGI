# Room 061: direct lighting below 16 ms

## Goal and acceptance

First milestone: live Composite, zero bounces, Room 0x061, root
`alttp-profile_backup.toml`, slot `Zelda_ALTTP.002.frz`, debug sphere off, M3 Max.
For the visible-torch stress case use **slot 006**, saved near the flames. Slot
002's initial viewport has zero visible emitters and does not test that workload.
Target **under 16 ms of overall frame-critical work**, not just median GPU time.
60 Hz provides 16.67 ms; use 16 ms as the deadline and aim for 12–14 ms to retain
headroom. The eventual goal is a stable 60 Hz presentation with one indirect
bounce across scenes. This milestone does not enable or optimize indirect light.

Acceptance must include frame-correlated CPU preparation, submission, GPU and
actual presentation timestamps, missed deadlines, skipped emulated frames and
in-flight-slot drops. CPU/GPU stages can overlap: neither summing unrelated stage
percentiles nor using GPU duration alone measures overall frame time. Distinguish
intentional synchronization sleep from active work and excess drawable waits.

Require at least 60 seconds stationary through every torch animation phase and
60 seconds of movement/scrolling, repeated three times with identical settings.
Report median/p95/p99/max, count over 16 ms and display deadline misses. The gate
is zero misses in these runs, not a claim that finite testing proves no drops in
every possible scene. Capture startup/profile changes separately from steady
state; room transitions are part of later whole-game acceptance.

## Evidence so far

October 6 live run, 180 completed samples (short, stationary):

| Measurement | Median | p95 | p99 / max |
| --- | ---: | ---: | ---: |
| GPU command buffer | 9.53 ms | 22.85 ms | 33.55 / 34.99 ms |
| Drawable acquisition | 0.013 ms | 10.57 ms | 17.90 / 19.03 ms |
| Field preparation | approximately 3.47 ms | see raw report | see raw report |
| Mesh preparation | 0.43 ms | 0.50 ms | 0.78 / 0.80 ms |
| GPU setup | 0.48 ms | 0.53 ms | 0.69 / 0.97 ms |
| Latest emulator refresh | 1.53 ms | 1.61 ms | not same-frame |
| Latest pacing wait | 8.16 ms | 8.49 ms | not same-frame |

Completion throughput was 43.05 frames/s with zero reported in-flight-slot drops.
That does **not** prove zero emulator skips or smooth displayed frames. The gap
between good GPU medians and poor throughput requires a critical-path/pacing
investigation before further blind shader tuning. GPU and drawable tails also
need correlation; drawable latency can reflect presentation back-pressure.

Prior deterministic run: GPU median 8.34 ms, p95 14.25 ms. Full sphere quadrature
with repaired batching: 92.11/124.73 ms. Distance-adaptive sphere samples are the
current baseline, not an experiment to re-enable indiscriminately.

## Prioritized experiments

### 1. Establish trustworthy end-to-end timing and isolate pacing

- Add frame IDs and correlated CPU start/end, prepare, drawable acquire,
  command-buffer commit/start/end and drawable-presented timestamps. Count
  emulated, eligible, submitted, completed and presented frames separately.
- Inspect emulator auto-frame-skip/pacing, synchronous waits, drawable/VSync and
  resource-slot ownership. Compare Original/no-lighting, direct-only and
  Composite0 with the same presentation settings and window/display refresh.
- Inspect GPU trace at slow frames; use stage counters for diagnosis and disable
  them for acceptance. Bin latency by animation identity, sphere count/budget,
  scroll changes and cold/warm resource events.
- Fix measured duplicate waits, serialization or frame-skipping decisions first.
  Do not remove necessary display pacing or hide slow frames by dropping them.

Deliverable: a trace explaining the 43 FPS result and every >16 ms class, with
same-frame end-to-end and presented-frame statistics.

### 2. Reduce physical sources with animation-aware fixture combining

- Combine neighboring emissive parts in the inspector after capturing a complete
  animation cycle; enable Representative Emitters. The authoring action now
  expands all known variants and completed-room offscreen sources honor links.
- A/B uncombined/combined profiles with the same emission data and identical
  animation/scroll schedule. Keep the user's original profile intact; use a
  separate candidate for measurement.
- Track visible patches/prepared records, offscreen spheres, dispatched sample
  budgets and actual ray counts, CPU build cost, GPU tails and image differences.
  A larger sphere can need more rays, so fewer sources is not automatically faster.
- Review near-source shadows, mixed colors, extinction and fixtures entering the
  viewport. Keep separate physical fixtures and preserve area/linear RGB power.

Deliverable: per-animation-phase source-count and latency deltas plus visual
review; adopt only if overall frame time improves without unacceptable flicker.

### 3. Remove repeated CPU field/source work

- Profile the approximately 3.47 ms field stage, distinguishing pixel decode,
  metadata lookup, normals, participation, emission and resource upload.
- Reuse static fields/geometry with explicit invalidation for profile revision,
  ownership/placement, scroll, animated content, lamp state and actor changes.
  Cache keys must include all inputs; animated emission does not necessarily
  require rebuilding static blocker geometry.
- Reduce allocation, redundant clears, hashing and copies before introducing
  complicated asynchronous work. Preserve bounded in-flight resource lifetimes.
- Measure the completed-room adapter separately; cache only with proven WRAM,
  graphics, profile and existing placement-base freshness.

Planning budget: approximately 2–3 ms total active CPU preparation/emulation,
with useful CPU/GPU overlap. This is a target, not a measured result.

### 4. Reduce GPU tail cost with shared direct-transport improvements

- Use the trace to rank direct transport, visibility traversal, source preparation,
  partial-buffer reduction and bandwidth. Tune the dominant stage first.
- Test tile/light work scheduling and conservative facing/occlusion rejection;
  skip only when zero contribution is established. Prioritize coherent shadow
  rays and fewer repeated mesh fetches. Preserve fractional blocker attenuation.
- Evaluate batch size, dispatch shape and scratch-buffer traffic at identical
  quadrature quality. Existing stripe batching regressed/no gain: do not repeat
  it without new trace evidence.
- If further approximation is necessary, measure an explicit error-bounded light
  clustering or spatial shading experiment, with edge-aware reconstruction and
  animation stability checks. Avoid a hard light-range cutoff or unreviewed
  temporal reuse that causes disappearing light/ghosting.

Planning budget: GPU p99 comfortably below 10–12 ms, leaving direct-only headroom
for eventual one-bounce work. Preserve the full-quadrature comparison override.

### 5. Long-run gate, then broaden scenes

- Run the stationary/movement gate after each adopted change, with stage counters
  and diagnostic readbacks off. Log every deadline miss with its failure class.
- Check torch phases, near/far lights, Link/objects, viewport entry, extinction,
  room transitions, profile edits and save-state restoration for correctness.
- Expand to other high-source/high-geometry scenes and whole-game traversal.
  Address worst-case shared algorithms rather than Room 061-specific light caps.
- Once direct performance passes, measure one-bounce cost separately against the
  remaining budget and build the indirect plan from that evidence.

## Verification for each experiment

Use portable emitter/sample/batch tests for changed CPU algorithms, the Metal
lighting regressions for transport changes, a Release app build, pixel/energy
comparisons where lighting changes, and live visual checks for animation/scroll
behavior. Record profile/state/build hashes, GPU/display/window settings and
counter overrides. Keep successful A/B artifacts and reject unmeasured wins.

See `remaster-benchmark.md` for runner/report details. Existing reports do not yet
implement the presentation/deadline instrumentation required by step 1.
