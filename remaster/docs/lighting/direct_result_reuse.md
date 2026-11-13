# Exact Direct result reuse

Each completion-gated Metal resource slot retains its most recent deterministic
Direct result. A second, immutable most-recent snapshot shares that exact result
across the three slots. Unchanged frames skip Direct transport, reduction and in-place
directional addition. Emission preparation and indirect lighting still run.
The indirect accumulation seed is explicitly reset on a hit; otherwise an earlier
even bounce could leak accumulated radiance into the next frame.

Reuse keys cover every effective input:

- geometry, occlusion, height/known state, surface and finite mesh;
- emission, participation, opposite-facing and reflectance fields;
- ordered effective source records, emitter patches, hidden samples and lights;
- receiver RGB albedo and Direct-affecting uniforms, source layout and serial mode;
- dimensions and device; slot identity is intentionally not a shared input.

The existing slot-local key uses exact field-upload baselines plus an ordered
input packet (not an explicit slot number). Sharing compares the whole packet
and all eight exact field/mesh byte arrays, never a metrics hash. Stable-seed
hybrid Direct already participates in exact reuse and shares under the same
rules; animated-seed/count diagnostics bypass both paths.

On a recompute, a blit after transport/reduction/directional addition snapshots
RGBA16Float bytes to a separately allocated texture. The serial presentation
worker publishes it only after committing its producer. Consumers require the
identical Metal command queue and a committed/scheduled/completed, non-error
producer, then blit the snapshot into their own slot. Metal's in-order command
queue execution and tracked-resource dependencies order this copy after its
producer. No uncommitted work is shared. CPU encoding/lifecycle is serialized by
renderMutex; snapshot publication/lookup also has a mutex shared with the
presentation worker. Generation checks prevent a late publication across a
texture allocation reset. Dimension/device changes and teardown invalidate the
snapshot. An already-encoded copy retains its immutable texture even if the
producer slot is subsequently overwritten/reallocated or a newer snapshot wins.
This deliberately avoids borrowing any rotating slot's mutable Direct texture.

`S9X_REMASTER_DISABLE_DIRECT_SHARING=1` disables cross-slot snapshots/copies while
retaining the original slot-local hits. `S9X_REMASTER_DISABLE_DIRECT_REUSE=1`
still disables both. A shared hit uses the existing hit branch, including the
indirect seed clear and skipped directional addition. First-use allocations
recompute conservatively; no quality, sampling or temporal policy changes.

Per-frame panel JSONL metrics additionally contain `hybrid_direct_enabled`
(effective kernel), `hybrid_direct_cutoff`, `hybrid_direct_tail_samples`,
`hybrid_direct_env_overrides`, `direct_reuse` (`hit`, `shared-copy`, `miss`) and
`direct_reuse_reason`. Unencoded/skipped frames report `miss/not-encoded`.
These are additive fields in the existing metrics object, not panel-global
asynchronous state, so each GPU completion retains its producing settings.

Uniform identity conservatively includes presentation settings; only the RNG
seed is excluded because exhaustive prepared Direct is deterministic. Sampled
Direct, GI measurement and diagnostic views bypass reuse. Allocation/dimension
changes invalidate it. Slot-local validity is revoked before encoding and
published only after successful command-buffer completion. Cross-slot snapshots
are published after commit and may be pending in order; failed producers are
rejected on lookup. Abandoned/uncommitted work cannot publish a shared result. `S9X_REMASTER_DISABLE_DIRECT_REUSE=1` forces the exact
recompute control. There is no temporal averaging or approximate source removal.

## Owner-accepted performance tradeoff

The owner explicitly accepted reuse on 2026-11-04 despite native dense reps1
GPU p95 increasing **13.83→17.48 ms** and refresh p95 **37.69→44.65 ms**.
Native dense GPU medians dropped approximately **12.73→0.186 ms**, **19.12→0.128
ms**, and **34.20→0.088 ms** at representatives 1/3/8. Recompute frames still
exceed the 5 ms worst-case CPU+GPU target; these are not additive CPU/GPU timings.
Tails coincide with nine-frame source-animation invalidation. Offscreen continuous
and periodic recompute controls do not reproduce the native penalty, so clock/
scheduling attribution remains unproved. Historical baseline-v2 is immutable;
an owner-accepted new baseline must be recorded separately through the stress
runner, not created by editing a prior report.

The owner-accepted record is local `build/performance-stress-suite/baseline-v3-reuse.json`,
created through the normal runner for all 18 pinned workloads with two processes
each. `baseline-v2.json` remains unchanged. Production revalidation passed 31
captures/eight outputs, both synthetic and pinned 23-case sanitizer regressions,
and the 52,016-check focused oracle. The clean curated gate passed 126/128;
only the known `visible_face_work` allocation and `tile_type_overlay` assertions
failed. The additional Metal regression accounts for the new test count.

## Rejected follow-up: incremental keys and complete-state LRU (2026-11-05)

A 16-entry per-slot LRU keyed by incremental 128-bit SipHash row digests (no
purge on geometry change) reached 92% hits on the dense `.008` route and cut its
reps1 GPU p95 18.35→13.25 ms with exact parity. It was reverted: screen-space
fields change every row on scroll, so digest maintenance raised CPU setup p95
about 1.0→2.7 ms on movement/exit routes (15 pinned stress regressions) while
those routes rarely hit. Evidence: local `build/performance-stress-suite/reuse-keys/`.
Per-ray/room-space visibility caching is the investigated successor.

Regression: `remaster/tests/direct_reuse_metal_test.mm` exercises the actual
renderer with 44 fixture-free input/bypass/sharing/lifetime checks and optional pinned capture
arguments. The curated `--metal` gate includes it. Historical evidence and frozen
proposals remain under local `build/performance-stress-suite/many-lights-opt/`.


## Private combined cross-slot validation (2026-11-05)

The private combined build passed44 renderer checks under ASan/UBSan and Metal
validation, including B/C byte parity against recompute with hybrid OFF/ON,
pending producers, producer-slot texture replacement/poisoning before consumer
execution, allocation invalidation, kill switch, and bounded-panel JSONL fields.
31 pinned captures ×8 output planes ×2 hybrid states were byte-identical to
pre-fix combined. Corrected room18 walk/circle routes passed640/510 frames per
hybrid state (18,400 output comparisons total); .008/.009/.011 gameplay hashes
matched in both states. The curated full+Metal gate passed128/130, with only
known visible_face_work and tile_type_overlay failures.

One live .008 baseline then one fixed process per hybrid state, representatives3,
gave median expensive-frame count per animation step2→1 OFF and4→1 at2%/8;
summed expensive-frame GPU medians42.81→19.50ms and254.45→16.48ms. GPU>2ms is
a whole-command-buffer diagnostic threshold, not a Direct-stage measurement.
Contention varied substantially, especially the hybrid baseline; results are
indicative only, no automatic reruns. Fixed outcome metrics counted136 actual
Direct misses in each1200-emulated-frame run (133 changed-input misses plus3
startup allocations). Hit/copy GPU scheduling outliers must not be interpreted
as additional Direct dispatches. Recompute still exceeds the5ms target.

Detailed distributions, denominators, hashes and cleanup are in local
`build/performance-stress-suite/combined/REPORT.md` and its
`evidence/direct-sharing/` ledger. This is not promotion into production.
