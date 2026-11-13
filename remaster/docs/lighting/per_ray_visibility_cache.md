# Exact per-ray visibility cache

The viewport cache retains **float32 visibility**, not light contributions, for
each receiver pixel and stable prepared-sample column. Exhaustive Direct and the
hybrid exact subset and sampled tail use the same cache lookup. Sample selection,
form factors, radiance, normalization and summation order are unchanged. Pairs
that hybrid never selects remain cold. Directional and analytic sphere rays do
not use this cache.

## Identity and invalidation

A pair occupies 16 bytes: float visibility, receiver version, sample version and
last transport generation. Receiver versions compare exact biased endpoints and
surface height; sample versions compare position, endpoint and normal bits,
excluding radiance. Receiver-dependent emitting-surface exclusions and planar
omnidirectional endpoints conservatively trace uncached.

Visible source identity uses exact patch membership/mode/stratum rather than
transient patch-array position. Hidden identities use exact geometry bits and a
duplicate ordinal, excluding radiance. Removed identities remain tombstones until
the next reset. There are no probabilistic hashes in validity decisions.

The room/plane/scroll lifecycle fields are populated independently of whether a
performance capture is active; disabling profiling must not silently permit
scroll retention. Offline replay packets without live camera provenance still
compare complete effective fields and remain subject to exact byte-parity gates.

Changed occlusion, height, surface and mesh cells contribute union-old/new bounds,
with the prototype's conservative XY/Z padding. An 8×8 receiver tile × singleton
source hull rejects unrelated changes; touched groups refine against each actual
biased segment. Old/new cell bounds are conservatively unioned per changed 8×8
tile to bound the group-test scan; no changed cell is omitted. Inactive pairs older than the previous transport generation trace
again, so skipped hybrid pairs cannot forget an intervening blocker change.

Room/plane/camera, device/queue/dimension changes and generation wrap reset the
cache. Rebuild/scroll frames bypass reads/writes. There is **no scroll retention,
guard band, offscreen history or room-coordinate tracer**.

## Queue and resource ownership

There is one cache across all three rotating renderer resource slots. GPU version
updates and pair writes execute on the presentation Metal queue in submission
order. CPU encoding is serialized by `renderMutex`. Per-command columns, change
bounds, dirty masks and counters are separate retained buffers, never overwritten
by another slot. Shared GPU records are tracked Metal resources. Allocation
replacement leaves earlier buffers retained by their command buffers.

A previous producer must be committed/scheduled/completed and non-error before
its CPU baselines may be consumed. Unsubmitted/abandoned or failed producers force
a conservative reset. Error callbacks invalidate their own cache generation.
Whole-Direct local hits and immutable cross-slot copies skip per-ray work; their
inputs need not advance the visibility baseline. The next miss compares against
the last actual visibility transport.

Capacity growth copies existing column-major pair records and sample versions on
the same queue before zeroing only new columns. Source insertion therefore does
not force unrelated columns cold. Dropping the cache on cap overflow is exact but
loses warmth. Command buffers retain replaced allocations until retirement; the
configured cap bounds one logical cache's accounted storage, not the transient
sum of retained old/new GPU allocations across in-flight growth/reset commands.
Driver/process RSS may exceed the accounted figure.

## Controls and memory

- `S9X_REMASTER_DISABLE_PER_RAY_CACHE=1`: disable cache (presence disables).
- `S9X_REMASTER_PER_RAY_CACHE_MB=256`: bounded allocation budget, default 256 MiB;
  integer 0–4096, malformed values use the default. Zero forces exact fallback.
- `S9X_REMASTER_PER_RAY_COUNTS=1`: diagnostic pair counters outside a profiling
  session. Normal gameplay avoids atomic counters unless metrics are requested.
- `S9X_REMASTER_PER_RAY_TIMING=1`: diagnostic timing-only override suppressing
  pair atomics in native benchmark mode only; outcome and memory remain available.
  Ordinary interactive panel captures ignore this override. Never use such a run as
  evidence of retrace counts. Ordinary panel captures keep counters enabled.

Pair memory is 16 × viewport pixels × retained columns. The cap also includes GPU
endpoint versions, dirty groups, worst-case changed bounds and CPU geometry
snapshots. Above the cap or on allocation failure, use the original uncached
tracer without reducing sample counts or changing the estimator. Reported bytes
are accounted storage, not process RSS or driver allocation overhead.

Per-frame bounded-panel and standalone JSONL carry `per_ray_cache`,
`per_ray_traced`, `per_ray_reused`, and `per_ray_bytes`, joined to the producing
frame. Direct-reuse frames have no per-ray work. Traced/reused counts describe
cache-eligible pairs, not exceptional uncached sphere/emitting-receiver rays.
Cause fields distinguish receiver version, sample version, stale/inactive pair,
and changed-segment invalidation (in that priority order). Counters use256 striped
receiver buckets to avoid a single global atomic bottleneck.

## Evidence boundary

The integration ledger is `build/performance-stress-suite/combined-prv/REPORT.md`.
Corpus parity is a required byte-exact gate, not a universal numerical proof of
every possible DDA contact. No timing or validation completion is implied by this
design document. Hybrid weighing still uses its original three scans; any weighing
optimization requires independent byte-exact output validation.

The measured visibility-bypass ablation leaves roughly3ms of hybrid transport
on the diagnostic `.008` sequence; the cache-enabled visibility path still costs
more than that. A future exact weighing cache could store receiver/sample factors
and power-weighted products, but radiance animation must update the products and
all three original scan/summation orders must be preserved (including adaptive
depth, exclusion, opposite-facing and sphere cases). A full float weight matrix
would add4×pixels×columns bytes and compete with visibility under the cap. It is
not automatically enabled or approximated by this port; optimize only after
counter-free native attribution shows weighing is the limiting stage.
