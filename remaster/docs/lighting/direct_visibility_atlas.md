# Conservative direct-light visibility atlas

The GPU visibility builder stores an `RGBA32Float` atlas of size
`width × (height + ceil(height/4))`. The first `height` rows contain each
pixel's conservative finite-mesh minimum/maximum Z, exact surface center and
known-height opacity. The bottom strip stores the existing 4×4 min/max envelopes
in its first `ceil(width/4)` columns. Unused strip columns are never read.

Pixel bounds use the same corner, center, shell-thickness and structural-wall
base arithmetic as the prior block builder. Transparent and unknown-height
pixels receive an inverted interval (`+infinity, -infinity`) and zero coverage.
Bounds are rejection data only, never solid columns or inferred geometry.

The existing pixel DDA and 4×4 skip remain. Before loading a mesh cell, traversal
rejects a pixel whose ray Z interval cannot overlap its conservative bounds.
Survivors use packed center/opacity and the unchanged finite-triangle predicate.
The origin-cell crossing still uses the original query. Endpoint biases,
translucent attenuation order/cutoff, source exclusion, source enumeration and
transport accumulation are unchanged. `REMASTER_REFERENCE_VISIBILITY` bypasses
both accelerators and packed fields, retaining the independent texture/triangle
oracle.

At 256×224, atlas storage increases from 28,672 to 1,146,880 bytes per slot
(+1,118,208 bytes; three slots add about 3.2 MiB). The builder runs only when
the existing geometry/field invalidation requires it. No new cross-frame cache
or relaxed dependency is introduced.

Validation: `remaster/tests/visibility_atlas_metal_test.mm` tests partial blocks,
signed heights, sheets, shells, wall bases, missing heights and fractional
coverage, then compares 40,960 deterministic rays with independent unaccelerated
triangle traversal. Run via the curated full `--metal` gate. The existing
`macosx/remaster-lighting-test.mm` retains full/half transport, endpoints,
transforms, self-exclusion and finite-shell regressions.

Room012 matched Xcode Release offline ABBA measurements (60 warm draws per leg)
showed baseline medians 35.19/30.98 ms and candidate 28.16/26.66 ms; combined
median 32.18 → 27.18 ms (15.5% lower). A separate matching-native-compiler-flags
ABBA measured 37.05 → 31.03 ms (16.2% lower). Complete transported Direct, Composite, lighting-field and mesh
bytes matched on 31 pinned real captures. Inputs, compiler commands, all cold/
warm/tail observations, rejected prototypes and remaining validation limits are
retained locally in
`build/steady-state-performance/room012/direct-visibility/REPORT.md`.
GPU contention varied; this is not a 60 FPS promise or a live-gameplay benchmark.

## Triangle rejection follow-up

The triangle predicate additionally returns as soon as its original u or v
barycentric predicates fail, before calculating the remaining intersection
terms. Arithmetic and triangle ordering are unchanged; the reference macro
retains full original evaluation. Matched Xcode Release room012_047 ABBA against
the accepted atlas baseline measured 24.58→23.52ms and a separate repeat
21.63→19.66ms (4–9% incremental GPU reduction, not a universal FPS promise).
All eight Direct/Composite/field/mesh outputs match on the same 31 captures;
independent ray and sanitized actual-renderer checks pass. No new resources,
quality settings, shadow approximations or CPU cache changes. Full follow-up
evidence: `build/steady-state-performance/room012/further-optimization/REPORT.md`.
