# Exact classification reuse at plane-only solve boundaries (2026-10-14)

Room060 / own capture071: **reviewable but incomplete**, performance equivalence
only. No geometry rules, placement datums, profile assets, publication boundaries,
or plane solve keys changed. No room-height approval. macOS build/presentation
and fresh interactive stair timing belong to the main agent.

## Change

Previously every actor-plane change invalidated the solve, cleared the artwork
states and reclassified both BGs. Oct7 retention avoided repeated classification
within a boundary, not between boundaries.

`remaster.h` now retains a heap-owned classification-input snapshot separately
from observed solve inputs. `S9xRemasterDungeonClassificationMatches` requires
exact complete map words, both collision arrays, complete template bytes/presence,
both graphics bases, placement BG index, ROM verification and every referenced
graphics byte. Profile/settings dirty events invalidate the snapshot outright;
profile hashes alone cannot authorize reuse after activation. Both signed local
placement grids are copied unchanged into invocation-local solve scratch.

Snapshots follow every successful replacement of retained classifications,
including artwork dependency refresh and exact artwork-state restoration. They
are not advanced speculatively before a solve succeeds. Actor plane/entry evidence
remain solve inputs, not classification inputs. No other-plane solved state is
retained/restored by this change; existing conservative artwork invalidation stays.
Additional bounded snapshot storage is approximately 103 KiB on the heap.

`ALTTP_TEST_DISABLE_CLASSIFICATION_REUSE` provides a narrow classification ablation.

## Regression / timing

New `dungeon_classification_plane_test.cpp`: eight alternating EndFrame planes,
natural solve invalidation, zero repeated classification, exact cached/full
two-BG map/diagnostic inventory for every plane, signed -256 relief, entry identity
changes, unrelated VRAM reuse, rejection for flips/priority/overwrite/attributes/
templates/referenced graphics/graphics bases/profile changes. Checks stay active
in Release (exceptions, not assert).

Existing capture probe: opt-in `ALTTP_PLANE_CACHE_BENCHMARK=N`. Each sample runs
the cached plane boundary, then disables classification and forces a full solve
for that same plane. Complete inventories and serialized packets must agree.
Only the first cached sample forces a boundary; subsequent plane switches must
invalidate naturally. Timings measure EndFrame, not serialization/comparison.
This is controlled fixed-artwork CPU replay, not a recorded live traversal.

Pinned input: `~/Library/Application Support/Snes9x/Remaster/frame_room060_071.s9xrmf`
plus own sidecars/profile, ROM SHA256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`,
profile serialized FNV1a64 `d381671ea7473c50`, rise50. Initial matching-pixel
replay denominator: 100465. No current profile substituted into this replay.

40 sample latest run: cached mean **34.9917 ms**, full mean **52.6944 ms**,
about **33.6% reduction**. Cached classifications0, full80; solves120 on each
path. Two complete plane inventories and every serialized packet matched.
Separate compile-time ablation run: 10 samples, classifications20/20,
solves30/30; both plane inventories byte-identical to enabled build.
Residual ~35 ms boundary cost remains; movement/geometric solving is NOT cached.

Commands (output parent must exist):

```sh
OUT=/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode
CAP="$HOME/Library/Application Support/Snes9x/Remaster/frame_room060_071.s9xrmf"
c++ -std=c++17 -O2 -DALTTP_TEST_CACHE_STAGES \
  remaster/tests/dungeon_geometry_cache_capture_probe.cpp -o "$OUT/room060-cache-probe"
ALTTP_PLANE_CACHE_BENCHMARK=40 "$OUT/room060-cache-probe" "$CAP" "$OUT/plane071"
# Optional compile-time oracle: add -DALTTP_TEST_DISABLE_CLASSIFICATION_REUSE.
```

Passed portable tests: classification_plane, placement_reuse, geometry_cache,
transition_cache, semantic_cache, disconnected_plane, entry_lifecycle.
Small-stack Debug (-O0 -g) and Release (-O2 -DNDEBUG), -pthread: synthetic plus
complete055/060/061 on both512/544KiB stacks (16 cases total).
Placement equivalence inventory: 055/060/061, both BGs, six flip/priority/overwrite
variants, 147456 placement records identical with classification ablation enabled
and disabled. This directly checks unchanged classifier output, not live caching
across every corpus state. Available pinned plane-cache differential corpus is
one own room060 state plus synthetic transformed/changed-input cases; no hundreds-
room/all-state generalization or gameplay claim. `git diff --check` passed.

## Complete-room gate (not passed)

Ran exact runtime fixture adapter and mandatory validator with full graphs,
ALTPPD1/ALTPAT1, state, compiled ALTPHM3 and verified v21 ROM. Historical obstacle
candidate is no longer accepted by current parser: tile belongs to multiple groups.
Did not alter that artifact. Reran using immutable runner copy of current root
profile. 055: exit1, supports439/439, missing0, conflicts0, walls1658, unknown28,
plane denominator[439,0]. 060: exit1, supports711/711, missing0, conflicts1,
walls1021, unknown9, planes[437,274]. 061: exit2, directional diagonal preview
at BG2(2,38) lacks independent ROM proof; explicit provenance/atlas retry also
rejects. These direct solver outputs do not exercise the new EndFrame cache;
cache equivalence is established separately by the complete inventory oracle.

Artifacts are temporary review outputs under `$OUT`: `plane071-plane*.inventory`,
`uncached071-plane*.inventory`, `placement-*-*.inventory`, `stair071.*` initial
replay, and `plane-fixtures/` runtime JSON, compiled maps, pinned profile/manifest,
055/060 validation JSON/SVG. 061 has runtime/compiled artifacts but no accepted
validation result. No height correctness or structural approval inferred.

Smallest next action: main agent build and fresh live trace across both stairs,
checking unchanged classification calls and remaining solve stalls; investigate
movement-component reuse independently without weakening plane/entry semantics.

## Follow-up: exact movement reuse (same date)

`frame.h` adds `RemasterDungeonMovementCache`, owned on the heap by one EndFrame
solve boundary and shared with BG1/BG2 and relation-enriched solves. Reuse requires
exact collision bytes, placements-present mode and the complete ORDERED seed list.
Seed order includes current actor plane/position and active-plane stair seeds;
movement is never reused from another frame or plane's cached solve. Only original
movement components and ordered projected-cell lists are reused. Persistent body
ownership, constraints, signed/entry datums, geometry and diagnostics still rerun.

The movement BFS also memoizes the exact leading-edge predicate by origin cell,
zero/nonzero X/Y pixel phase and direction. All existing sample offsets are0,8,15,
16,23,24, so these four phase classes reproduce exactly the same sampled cells.
Full-body projection similarly skips only repeated identical support-cell spans;
first visit and component ownership ordering remain unchanged. Already visited
destinations reject before redundant collision sampling. Scratch is heap-owned
and invocation-local. `ALTTP_TEST_REFERENCE_MOVEMENT` disables ALL these changes
for an independent compile-time oracle.

Cold classification: outer-door-band probes stop on the first exact rejection,
including height-sample loops. No ownership/matching/classification rule changed.

Latest40 `_071` alternating samples: **21.211 ms** cached-classification boundary,
versus **34.7545 ms** with reference movement (38.97% reduction). Forced
classification misses:38.8305 versus52.4982 ms. Three geometric solves per frame
remain (120 each), classification0 vs80 as before. Enabled/reference builds have
byte-identical complete inventories AND serialized packets for both planes;
each run also checks cached/full-classification packets at every sample.

**Not a smooth-stair claim:**21ms still exceeds16.67ms and fixed-artwork CPU replay
is not live chronology. Cross-frame exact solved-state reuse was deliberately not
added: Link position seeds can change traversal ordering/component diagnostics;
the existing solve hash omits position. Restoring by plane/hash alone would not
provide the required exact oracle. Position-keyed solved states would not address
general repeated traversals at different positions, without a proved canonical
movement/component ordering or seed-independence capability.

Extended regression exercises all64 pixel phases, alternating/disconnected and
plane-changing-stair scope, directed doorway attributes, entry changes and signed
datums, repeated exact movement scratch and full inventory oracle. Portable suite
also passed signed_pit, persistent_support_rejection and obstacle_support_component.
All16 small-stack cases rerun successfully. Full runtime JSON matches the reference
movement build byte-for-byte on055/060/061. Mandatory fixture/compiled-map validators
rerun: identical055/060 exit1 counts and061 exit2 proof rejection documented above.
Build/fresh interactive validation remain assigned to the main agent. Output
prefixes under `$OUT`: `movement071`, `reference-movement071`,
`movement-runtime-{055,060,061}.json`, `reference-runtime-{055,060,061}.json`.

For the reference timing/inventory/packet oracle, add
`-DALTTP_TEST_REFERENCE_MOVEMENT` to the capture-probe compile command and use a
distinct output prefix. Compare both `-plane{0,1}.inventory` and
`-plane{0,1}.s9xrmf` files with `cmp`. Main agent can build now; runtime movement
optimizations are complete within this bounded safe scope. Next capability for
further savings: canonical movement-component representation (proved invariant
to actor seed/order) or independently profiled geometric solve intermediates,
not weaker plane/entry invalidation.

## Integration verification

Main agent built both Release and Debug with the shared `snes9x` scheme,
`-destination "platform=macOS,arch=arm64" CODE_SIGNING_ALLOWED=NO`; both passed.
Updated Release app: `build/release/Snes9x.app`. `git diff --check` passed.
Fresh interactive traversal and presentation/GPU timing were not performed.
The captured CPU boundary improvement does not establish live 60 FPS.
