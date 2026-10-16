# Pixel-derived triangle occlusion

## Emission presentation

### Flame emission direction

Emission mode now has an undoable, multi-edit **Omnidirectional Emission** checkbox.
It decouples direct source angular response from the authored shading normal, so
zero-depth flames can illuminate floors without pointing their normals down.
Receiver cosine, surface reflection, and visibility still apply; depth samples
still extend along the authored normal. Positive depth retains its existing
omnidirectional volume behavior. Reflected light in later bounces remains diffuse
surface transport. Rectangular and representative patches keep distinct direction
modes separate, including explicitly combined multi-tile emitters.

Profile schema19 adds per-asset `emission_omnidirectional = true`; frame schema27
appends a validated boolean after each asset's role. Older files retain directional
zero-depth emission. The option follows Emission layer/full-tile copy and reset,
animation metadata copying, undo, and profile save/replay.

Zero-depth omnidirectional sources choose their visibility endpoint toward the
receiver using geometric shell thickness, independently of shading normals.
This fixes opaque elevated flames self-shadowing toward the floor when their
normals point +Z. Receiver endpoints and intervening blocker attenuation retain
their surface semantics; positive-depth geometry still follows authored normals.

Direct Light and Direct + Indirect views now include visible self-emission and
use Composite's exposure and hue-preserving highlight mapping, without the
original scene contribution. Indirect-only remains a reflected-light diagnostic.
Emissive pixels also receive direct/reflected illumination through normal material
response. Their self-emission replaces baked artwork brightness in Composite,
preventing white baked highlights from washing out low-intensity orange glow.
The inspector preview now uses intensity/25 and shared-RGB compression, matching
runtime emission rather than the old intensity*4/channel-clipping preview.

Validation: direction/profile/frame round trips, legacy frame compatibility,
mixed-direction patch isolation, and 1294/1294 Metal checks (full/half float,
sampled/exhaustive, rectangular/representative, normal independence, receiver and
blocker behavior, opaque emitter shells, lighting-only self-emission, baked-white
replacement and zero-original Composite). Preview checks cover all intensities
1–255. Interactive scene appearance still needs review.

Authored intensity byte 25 still seeds 100 linear radiance units for transport.
The visible emitter uses artwork-scale radiance (transport divided by 100), so
raising its lighting power no longer applies that transport calibration directly
to its displayed flame. Composite and Emission views compress highlights above
0.75 linear using one shared RGB scale, preserving channel ratios instead of
clipping warm highlights to white. Direct and Direct + Indirect presentation use
the same mapping. Transport buffers retain their existing HDR energy calibration.

Profile refreshes preserve capture-local `capture_animation_*` groups and instance
membership, so replay Previous/Next browsing survives loading or editing a profile.
The room061_001 capture contains nine such tracks, including three-frame tracks.
Its stored metadata has no authored emission, and its stored scene settings have
zero indirect bounces; reproducing subsequent emission edits requires the matching
edited profile. Light transport has inverse-square-style falloff, with no fixed
torch range cutoff. Extra bounces are needed for indirect room fill.

Validation: Metal lighting harness 1254/1254 checks, including high-intensity flame
color/transport separation and final-composite agreement; macOS arm64 Debug build.
All nine actual room061_001 animation tracks and their instance memberships survive
three successive profile refreshes. The broad portable suite is blocked by legacy
fixture assumptions (11 profile groups versus the current 12, and old-version
binary conversion); its new synthetic animation-refresh assertions pass.
Interactive scene appearance and inspector navigation still need visual review.

## Sampled Direct lighting

### Distance-adaptive sphere quadrature

Physical `frame.lights` use deterministic 8/16/32/64/128-sample solid-angle
quadrature according to angular radius: below 0.01/0.025/0.05/0.1 radians uses
8/16/32/64, otherwise 128. CPU preparation bounds the budget using distance to
the nearest viewport pixel center; the shader refines it at each receiver using
3D distance. Near/inside spheres retain 128, and the session debug sphere always
retains 128. Visibility and GI diagnostic paths retain full quadrature.

Every retained sample uses `2 * coneExtent / sampleCount`; source power, receiver
response, fractional blocker attenuation and the existing normalization clamp
remain. Reduced quadrature approximates penumbra edges and facing integration;
it does not impose a light range cutoff or reuse temporal history. Sampling is
deterministic, so there is no per-frame random noise, but budget transitions can
change shadow edges. `S9X_REMASTER_FULL_SPHERE_SAMPLES=1` at launch restores 128
samples for all lights for comparison.

Exhaustive transport now grows samples per batch when its float32 partials would
exceed 64 MiB per in-flight slot, rather than falling back to serial transport.
All sources remain represented; summation grouping can change rounding. Metrics
include sphere count, maximum dispatched sphere samples, and direct batch size;
the actual per-receiver ray count can be lower after angular/facing rejection.

### Single-record sampled emission depth

Production representative Direct additionally uses deterministic distance-adaptive
depth quadrature: retain four strata when depth/distance is at least 0.05, otherwise
use two midpoint strata with twice the quarter-stratum RGB and normalization area.
All spatial/color representatives remain; this only reduces depth rays at each
receiver. Expanded source records retain negative depth in `normal.w`; sampled
collapsed records retain positive depth, with expanded sampled endpoints ignoring
the negative metadata. Diagnostics and unclustered reference transport remain full.
`S9X_REMASTER_FULL_DEPTH_SAMPLES=1` at launch restores all four depth strata.
This approximates distant depth shadows and can change shadow edges/animation
transitions; it does not preserve every blocked ray exactly. The Direct Samples
metric still counts prepared records, not the lower executed ray count.

Sampled Direct now prepares one base record per pixel/patch, including positive
emission depth. Exhaustive Direct retains four quarter-energy depth records.
The source record's high bit in `y` marks sampled base preparation; it is masked
before patch indexing. `normal.w` carries depth, with unshifted shading position
and biased ray endpoint, full RGB, and full normalization area. Patch receiver
exclusion subtracts full member energy/area and base centroid weight first.

Each scan evaluates the existing four depth form factors together at offsets
`depth * (0.125, 0.375, 0.625, 0.875)` along the authored normal. Their mean times
source power determines source-selection probability; their mean times area
contributes to the exact unoccluded normalization. For each selected connection,
a separately salted random draw selects a depth stratum proportional to its
factor. The joint-probability estimator uses full RGB/power times total weight;
no additional quarter-energy or form-factor multiplier is applied. Repeated
selections of the same source and stratum share a visibility query.

This preserves the expected four-stratum transport, not a continuous-volume
integral. Source/stratum random choices change, so individual frames differ from
expanded sampling; there is still no temporal denoiser. The four cheap factor
calculations remain, making record-count savings larger than total timing savings.
`S9X_REMASTER_EXPANDED_SAMPLED_DEPTH=1` restores expanded sampled records for A/B.
Exhaustive, visibility/GI diagnostics, and enabled debug-sphere paths automatically
retain the expanded representation. No profile or capture schema change.

### Emitter patches

**Explicit multi-tile emitters:** in the tile inspector's Emission layer,
Cmd-click two or more tile types in the scene and choose **Combine Emitters**.
All selected types must occur in one cardinal-edge-connected shape in the current
frame (a2x2 shape is valid; diagonal-only contact is not). Otherwise an alert is
shown and no metadata changes. The button is disabled for one selected type and
has a selection reminder tooltip. The action applies live, is undoable, and saves
reciprocal `combine_emitters` tile-hash lists for all known animation variants
of each selected type in asset metadata (profile17,
capture24). Older profiles/captures have no links. Existing links are retained
when adding peers; use Undo to reverse the authoring action.

With representative mode enabled, mutually listed tile types join only when their
actual8x8 footprints share a cardinal edge in the selected screen channel and
source domain (backgrounds also require the same BG layer). Non-emitting pixel
gaps do not break adjacency; disconnected copies never join remotely. Tile flips
and clipped visibility are accounted for when reconstructing footprint origins.
Inconsistent origins are excluded from joining. Only explicit hash peers are
allowed. Captured animation tracks take precedence over semantic asset groups;
each part's variants are linked to all peers so asynchronously animated parts
also combine. Frames not present in the capture/group cannot be inferred: capture
a full animation cycle and repeat Combine Emitters when discovering new variants.
Previously saved current-frame-only links are expanded by repeating the action.
Selection is by
tile type, not individual occurrence; one connected occurrence containing every
selected type is enough to validate the action.

Each connected shape sharesK representatives rather thanK per tile. Summed RGB,
normalization area, actual-member endpoints and receiver exclusion are retained.
GPU membership capacity is256 (1032-byte CPU/Metal patch ABI), covering four full
tiles even atK=1. Larger shapes cluster together but any resulting cluster over
256 members is split to retain every source, potentially exceedingK. Off retains
the existing rectangular patch algorithm; authored links take effect when
representative mode is enabled. Identity links are not transferred by paint-layer
copy/paste or Apply to Animation, and normal/height resets retain them. Reset Tile
removes that tile's links; reciprocity prevents leftover incoming links merging.

Completed-room offscreen sources also honor reciprocal links when representative
mode is enabled. Cardinally connected missing-pixel placements on the same BG
plane reduce to one sphere per connected shape, retaining total emitting area,
intensity-weighted center/height and linear RGB power. Disconnected fixtures,
one-way links and different emission-direction modes remain separate. This is
the existing offscreen sphere approximation applied to a larger fixture, not
an exact preservation of its near-field shadows. Visible pixels remain exclusively
in the visible path; partially visible shapes can still have both visible
representatives and a missing-pixel sphere. Larger merged spheres can require
more angular samples, so source-count reduction must be benchmarked rather than
assumed to improve frame time.

Scene Controls also offers opt-in **Representative Emitters per Tile**: enable
the checkbox and choose1..8, initially4. Off retains rectangular patch grouping;
patch-size controls are disabled while representative mode is enabled. Profile
schema16 stores `representative_emitters` (0=off); frame schema23 stores the same
byte. Older files default off. Changes support live apply and scene-settings undo.

Representative mode clusters visible emitting pixels separately for each selected
main/sub channel, owner and tile instance. Deterministic six-iteration weighted
k-means uses canonical tile XY/7 and max-normalized linear RGB with equal feature
scales. Highest-power first seed, power-weighted farthest subsequent seeds, and
canonical-order ties keep identical inputs repeatable. Power includes intensity.
Every active member appears once; holes/nonemitters never become members.
For ordinary8x8 instances the slider caps clusters atK. Oversized malformed or
duplicated occurrences are split into64-member chunks, each capped atK; unresolved
identities stay singleton. These fallbacks can exceed the per-tile cap.

GPU clusters preserve summed linear RGB and member-count normalization area.
Their representative is the actual member nearest the power-weighted3D surface
centroid. Shading position/normal, geometric biased endpoint and depth all come
from that member. No averaged endpoint is placed in a hole. Receiver exclusion
rebuilds the remaining representative and energy/area. Expanded layout uses four
quarter-energy records if ANY original member has depth; layout remains fixed
when exclusion changes the chosen member. Collapsed sampled mode uses one record.
Mixed geometry/directional/depth behavior is deliberately approximated by the
chosen member, so near-source shadows and animation can shift/pop. This mode
does not change artwork emission or blockers. Visual acceptance remains pending.

Scene Controls' **Emitter Patch Size** is an integer1..5 maximum footprint,
default2. Profile schema15 saves `emitter_patch_size` in `[lighting_space]`;
capture schema22 saves it for replay. Older profiles/captures default2. Size1
retains per-pixel emission. It affects exhaustive and sampled Direct, not the
displayed emission artwork or blocker geometry.

Deterministic full rectangles (including1xN strips) merge only within the same
owner instance and canonical tile-local N-grid bin. Hidden/nonemitting pixels
never fill holes. Height, shading/geometric normals, flat corner support,
thickness and depth must match. Colors split when max-normalized linear RGB
differs from the seed by more than0.4 in any channel. Summed linear radiance
preserves RGB energy; maximum-channel power weights positions and biased
endpoints. Normalization area is member count, quartered for depth strata.
Receiver membership removes only that receiver's energy/area and centroid weight.
Visibility/diagnostic modes bypass clustering. Snapshot export currently requires
size1 because its packet format does not contain member lists.

This is approximate area-light transport: a centroid cannot preserve all nearby
shadow boundaries, inverse-square falloff or spatial color variation. Larger
footprints are maxima, not guaranteed source counts; compatible fragments often
remain smaller. Visual acceptance of clustering is pending.

Normal Direct is deterministic and exhaustive over prepared emitter clusters
and adaptive sphere quadrature. Scene Controls connections per receiver controls
indirect lighting only. Explicitly opt into stochastic Direct with
`S9X_REMASTER_DIRECT_SAMPLES=16` (valid1..4096; unset/invalid keeps deterministic
Direct). `S9X_REMASTER_EXHAUSTIVE_DIRECT=1` overrides that experiment.
Visibility/GI diagnostics and the debug sphere remain exhaustive.
The implementation stratifies samples over each
receiver's source CDF, weighted by unoccluded Lambertian form factor times the
maximum linear radiance channel. It scans all prepared planar/depth sources to
compute the **exact unoccluded normalization**, then scans their weights again
to choose a bounded number of visibility queries. Repeated selections of the
same source share one visibility query with multiplicity weighting.

For source `i`, `w_i = f_i * max(L_i)`, `W = sum(w_i)` and `p_i = w_i/W`.
Estimate `sum(L_i*f_i*V_i)` with `L_i/max(L_i) * W * V_i` per selection, averaged
over the requested count. Normalize with the exhaustive `sum(f_i*area_i)` and
the existing0.95 clamp, then apply receiver albedo once. Depth radiance and
normalization retain their quarter-sample energy convention. This avoids a
stochastic ratio/clamp denominator; the estimator targets current exhaustive
lighting in expectation, subject to float/half rounding.

Random seeds advance with rendered frames; fixed benchmark seeds reproduce
results. There is no temporal reuse, denoiser or persistence yet, so live shadows
can show sampling noise/flicker. Exact source scans remain O(receivers*sources).
Indirect connections remain independently controlled by the profile. Visibility
and diagnostic views and an enabled debug sphere use exhaustive Direct. Source
and geometry preparation remain frame-local and unchanged.

HUD source-only pixels use the existing rectangle/representative clustering
settings on CPU-only 8x8 support grids. Their deterministic depth records retain
four quarter-power strata and the same receiver-adaptive depth refinement as
visible clusters. No support grids enter scene geometry. In the explicit sampled
experiment these become collapsed depth base/extent records. Physical scene spheres also join the CDF,
using receiver-adaptive solid-angle quadrature; their unoccluded form factors
and normalization match exhaustive sphere transport. Sphere setup is cached per
light per scan. See `offscreen_emitters.md` for capture timings and regressions.

## Connected surface meshes

### Mesh diagnostic

Cycle lighting views with Command-Shift-D to **Mesh** (after Emission). The
diagnostic draws the actual generated center-fan triangles, shell bottoms/sides,
and structural wall closures over dim artwork at 4x source resolution. Cyan is a
sheet, amber a finite shell, pink a base-closed structural wall. Transparent
receivers are dimmed; unknown-height pixels have no triangles. A fixed oblique
projection `(x + .35*z, y - .65*z)` separates elevated geometry from its original
screen footprint. Depth testing retains the frontmost faces. This is a geometry
inspection view, not an editing coordinate transform: selection still uses the
original screen coordinates. Mesh presentation bypasses light transport and runs
synchronously; it is intended for paused inspection, not performance measurement.

`surface_mesh_capture_probe.cpp` exports the same view and a per-pixel mesh CSV:

```sh
clang++ -std=c++17 -O2 remaster/surface_mesh_capture_probe.cpp -o build/remaster-tests/surface-mesh-capture-probe
build/remaster-tests/surface-mesh-capture-probe capture.s9xrmf Profiles/ALttP/alttp-profile.toml build/remaster-tests/scene
```

Courtyard capture `frame_overworld_x0868_y0715.s9xrmf` (schema35, scale200)
demonstrates a mixed-semantics boundary: tree pixel (124,144), hash
`1f1ef598c2067de7`, local sample0 is authored height0 but tile role Obstacle,
therefore a unit shell with top at +0.5. Floor sample (124,128) is height0 with
role Floor, therefore a sheet at0. Matching shading normals do not eliminate
this physical step. The tree canopy is disconnected from its ground samples;
there is no single whole-tile triangle spanning those heights. The supplied
capture has no scene-pixel appendix and many ordinary grass pixels have unknown
height, which further limits the represented ground. Across the complete frame,
540 opaque known zero-height samples are sheets and718 are shells;128 shell
samples have nonflat reconstructed corners (not all belong to the tree).

This diagnostic does not repair the ground outline. A general repair needs
pixel-level ground/obstacle semantics within mixed tiles (and known surrounding
ground), or a separately specified shell/ground contact convention with transport
regressions. Height zero or +Z shading alone is insufficient evidence to turn
arbitrary sprites, bars and props into floor sheets. Existing heights/normals and
production visibility geometry remain unchanged by this view.

Validation:39/39 portable mesh checks, including sheet/shell visualization,
unknown exclusion, deterministic rasterization and full/cache equivalence;
macOS arm64 Debug build passed. Offline capture export visually inspected;
interactive app cycling/presentation has not yet been checked.

Visibility now intersects triangles. `remaster/surface_mesh.h` constructs a
compact 48-byte patch per screen pixel; Metal expands its corner heights into
four triangles meeting at the authored pixel center. Shading positions remain
on the mesh and silhouettes retain full pixel footprints. The pixel grid and
4×4 height envelopes accelerate candidate lookup rather than defining geometry.

The default shader also rejects shell/wall side-face candidates whose
XY plane lies outside the patch ray interval, using the fixed margin
`0.001 + abs(rayAxis)*2e-6`. The user visually reviewed this faster variant and
accepted it as the default on October 1, 2026. Top/bottom fans and surviving triangle predicates/order stay
unchanged; `REMASTER_REFERENCE_VISIBILITY` bypasses the filter. Bounded regression
and pinned-frame outputs match, but extreme-coordinate/near-parallel numerical
equivalence is not guaranteed. The more heavily guarded variant was slower.

Floor-class background pixels connect across tile hashes and draw records.
Wall faces and ordinary wall tops share a wall domain, including corners;
`stair_rails` remains finite geometry. Continuous floor/wall patches are thin
sheets. Other pixels use finite-thickness triangle shells, retaining the previous
one-unit extent for discrete details, bars, and sprites. Unclassified backgrounds
may connect within a BG layer but retain thickness; background props stay within
their draw instance. No triangles join an OAM object to a background.

OAM object domains use the frame-local source slot. Separate slots never weld,
even when touching or sharing materials. Frames do not carry logical NPC/pot/
projectile IDs; a multi-slot actor therefore remains multiple independent pieces.
Whole-actor grouping requires a ROM-specific object-to-OAM mapping.

Connectivity requires known heights, nonzero opacity, matching domains, and
compatible height continuation. Small quantized steps connect; larger ramps need
matching continuation evidence. Abrupt ledges remain hard boundaries. Corner
vertices weld within cardinally connected local regions; diagonal-only contacts
and transparent holes do not bridge objects. Disconnected levels are not stretched
into ramps. Floors, rails, and props are not automatically extended downward.
Monotone slope reconstruction preserves linear ramps without inventing raised
ridges where a flat plateau meets a descending edge.

Structural wall faces/tops close their sides down to the placement floor base
(`heightOffset × coordinateScale / 255`). The visible smooth ramp stays unchanged
while the otherwise open space below it closes. Solid-wall endpoints always bias
toward the exterior geometric normal, independently from artwork shading normals.
Side closure uses the surface's opacity and participates in acceleration envelopes.

Solidity is classified by material and placement, never by authored shading
normals. `remaster/surface_mesh_material.h` shares this classification between
the renderer and regression tests. Unclassified elevated bars retain finite
shells even when touching a structural wall. A previous normal-driven frontier
incorrectly closed the jail crossbar `v1:4bpp:c43a8c5e3814af21` down to the floor
and has been removed. Genuine unclassified walls require structural semantics
before receiving base closure. Height metadata is not rewritten.

The earlier captured jail leak checks used that removed frontier; their blocked
ray counts are historical results, not validation of the current classifier.

Wall corner ramps encode inside/outside folds in their height arrays (max/min
composition). Geometry follows those heights, placement offsets, and displayed
flips. Neighboring face normals need not be parallel to connect a corner.
Authored shading normals remain per-pixel and transform once through the existing
instance transform; geometric normals are separate. Missing shading normals use
the mesh normal instead of gradients across unrelated objects.

Sheet endpoints use a small geometric-normal bias toward the authored shading
side. Only endpoint contact is excluded: a proper sheet crossing within the
receiver cell still blocks a light behind the wall. Finite-shell endpoints keep
their authored XY center and offset Z to the actual top/bottom face; shading
normals select the side. The old dominant-normal voxel offset could leave rays
inside flat shells with tilted artwork normals, causing false tabletop seam
shadows. Known surfaces receive the same face offset regardless of blocker
opacity, preventing transparent tabletop receivers from starting inside their
opaque neighbors' coplanar shells. Proper crossings within source/receiver cells still block, including
light below an opaque tabletop. Each intersected pixel patch attenuates once even if multiple
triangles are hit. Flat-shell tangencies remain nonblocking. Direct, sampled
indirect, and Visibility share this query.

Validation: 33 portable mesh checks; 1215 Metal checks including 456 accelerated/
reference comparisons, shallow wall lighting, opposite-side blocking, floors,
emitter sheets, bars, gaps, and mixed sheet/shell geometry. Elevated height-50
bar regressions test underpasses, opacity at the actual bar height, clearance
above it, and floor sphere illumination with vertical/lateral authored normals,
including a touching structural wall and both float/half-float GPU targets.
Height-5/radius-2 sphere regressions additionally verify continuous opaque
structural dividers block direct light and explicitly transparent walls do not.
Synthetic M3 Max
256×224 one-bounce benchmarks measured complete sampled GPU pipelines around
0.6–3.0 ms (4 connections), 1.0–6.2 ms (8), and 2.4–13.2 ms (16), excluding CPU
mesh preparation. These are not live-room frame timings. All four benchmark
scenes matched accelerated/reference output exactly. Dense reference transport
is much slower; live indirect uses the sampled path.

Visible samples cannot reconstruct hidden/offscreen geometry or independent
overlapping planes. This renderer change does not assign or approve room heights.

## Historical voxel contract

The following records the prior voxel model and floor self-shadow fix. Flat
finite-detail triangle shells preserve its bounds and regression behavior;
continuous floor/wall sheets instead use the mesh rules above.

Lighting represents each visible, height-known pixel as a finite axis-aligned
voxel. For screen pixel `(x, y)` and effective physical height `z`, its bounds are:

```text
X: [x, x + 1]
Y: [y, y + 1]
Z: [z - 0.5, z + 0.5]
```

The effective height combines the tile's local height (or placement override)
and instance height offset, then converts the result using the lighting coordinate
scale. Voxel thickness is one source-pixel-equivalent world unit, independent of
that scale. Bounds are not clamped at height zero or the maximum authored height.

Direct illumination, the debug sphere light, visibility diagnostics, and indirect
transport share the same segment–voxel intersection test. Pixel-grid traversal
provides each voxel's XY slab interval; intersecting that interval with the Z slab
completes the ray–AABB test. Only a positive-length intersection attenuates light.
The source and receiver XY cells are excluded to avoid self-shadowing. Exact XY
corners advance both grid axes, and boundary-aligned rays use one half-open grid
lane instead of double-counting neighboring cells.

Shading heights describe voxel centers, but surface visibility endpoints must
lie on their outward faces. For a represented, height-known, nontransparent
surface, transport offsets its endpoint along the surface normal by
`0.5001 / max(abs(normal))`. This reaches the unit voxel's outward face with a
small numerical clearance. Both receiver and authored-source endpoints use this
rule in Direct, indirect, and Visibility views. Analytic sphere endpoints stay
exact; shading distances and cosines still use authored surface centers.

Starting rays at centers caused opaque floors and emitter sheets to shadow
themselves: shallow rays intersected neighboring coplanar voxels before escaping
the half-unit thickness. This produced square floor-light footprints and removed
distant torch illumination. Face endpoints fix that without changing blocker
bounds or making raised geometry transparent.

An intersected voxel multiplies visibility by `1 - occlusion / 255` once.
Transparent pixels and pixels without known effective height do not block rays.
The existing very-low-transmittance cutoff remains in use. Surface normals and
light reception are independent of blocker opacity.

This replaces the previous downward-solid height-column model. An opaque raised
crossbar now blocks rays through its actual volume, while rays below or above it
remain clear. Horizontal and vertical jail bars can therefore retain full opacity
and cast shadows without closing the transparent openings between them.

The 8×8 acceleration blocks store the minimum voxel bottom and maximum voxel top
in an RG32Float texture. A ray can skip a block when its height interval is entirely
above or below that envelope. The envelope is not itself solid: overlapping
intervals still require individual voxel intersection tests.

## Validation

- `remaster/indirect_lighting_reference_test.cpp` provides a portable CPU oracle
  for finite voxels, ascending/descending rays, tangencies, fractional coverage,
  missing heights, endpoint exclusion, and world-unit thickness.
- `macosx/remaster-lighting-test.mm` tests the production shaders in float and
  half-float radiance, grid openings, direct/debug/indirect transport, and
  accelerated versus unaccelerated traversal. Continuous opaque-floor fixtures
  cover shallow sphere rays, authored emitter sheets, indirect transport, and
  raised blockers (861 checks); the CPU oracle passes 43 checks.
- The model uses represented visible pixels. Hidden and offscreen geometry, or
  multiple surfaces at the same screen coordinate, require additional geometry
  data beyond the current composited frame.

## Possible future improvement: volumetric light shafts

Grid openings currently produce illuminated regions and shadows on receiving
surfaces. Visible shafts in empty space would require a participating medium,
such as fog or dust, with volumetric scattering and extinction. Reuse pixel-voxel
visibility for medium-to-light connections if that feature is added; it should
remain separate from surface illumination.

## Authored emission depth

Direct transport prepares a compact source-sample buffer once per presented
lighting frame. Planar emitters contribute one record; depth emitters contribute
four normalized records. GPU preparation stores source positions, visibility
endpoints, normals, and radiance, preserving source order and the existing
transport equations. Buffers are reused per in-flight resource slot. The dense
unprepared path remains available for reference comparisons and diagnostics.

Production Lambertian Direct additionally specializes `remasterIndirectBounce`
with Metal function constant 0 (`remasterPreparedDirectPass=true`). All dense
reference/diagnostic uses explicitly set it false; Visibility retains diagnostics.
The specialized shader removes radial indirect sampling and diagnostic stages at
compile time while retaining the same triangle visibility query and emitter
enumeration. Float/half regression cases exercise this production specialization.
The seven-case slot-004 benchmark and explicit rounding tolerances are recorded
in `macosx/remaster-benchmark.md`.

The October 1 visibility follow-up reduces envelope blocks from 8×8 to 4×4
and streams successive triangle edges rather than dynamically indexing a local
vertex array. Coordinates, triangle order, opacity, and endpoint rules remain
unchanged; `REMASTER_REFERENCE_VISIBILITY` retains the original array expansion
and unaccelerated traversal as an independent oracle. All 1215 checks pass.
The final-build slot-004 sequence measured Direct median 66.39 ms and one-bounce
median 105.38 ms, with identical end-of-case PNGs versus the previous implementation.
Benchmark-only Metal timestamp counters now isolate direct/indirect transport,
source-power construction, emitter preparation, envelope construction, and
compositing. See the benchmark note for comparisons, limitations, and commands.

Production transport now additionally partitions Direct's exhaustive source list
and indirect's deterministic connection list into eight-element GPU batches.
Each batch stores float32 incoming RGB (and Direct's normalization form factor),
then a separate reduction applies the existing normalization/albedo/accumulation.
Every source/connection is retained; no distance cutoff, light clustering, temporal
reuse, or reduced-resolution receiver shading is introduced. Summation grouping
changes floating-point rounding. Serial transport remains available for diagnostics
and via `S9X_REMASTER_SERIAL_DIRECT` / `S9X_REMASTER_SERIAL_INDIRECT`. Reusable
per-slot partial buffers are bounded to 64 MiB; larger workloads fall back to serial.
Metal function constants 1/2 select batched direct/indirect; sampled-indirect
pipeline creation must use `constantValues:` even for the serial variant.
The same eight-element batch size must be used by app, test, and shader dispatch.
Both serial and batched lighting suites pass 1215 checks. Final slot-004 medians
are Direct 22.00 ms, one bounce 37.97, eight bounces 101.60 at 96 connections.
See the benchmark note for live-mode differences and output-rounding validation.

CPU geometry reconstruction now uses `surface_mesh_cache.h`: a screen-space cache
compares every resolved sample (height, known, coverage sign, domain, sheet,
structural wall fields). Changes dirty a three-pixel neighborhood, grouped into
16×16 output tiles. Dirty tiles are rebuilt with a further three-pixel input halo
using the original builder, preserving raw-corner reconstruction and weld order.
Dimensions/quantum changes and widespread edits use a full build. Metadata normals
and emission depths are refreshed after reuse; all GPU slots still receive complete
uploads. `S9X_REMASTER_FULL_MESH=1` selects the independent full-build path.
No static-tile flag or offscreen geometry retention is assumed; moving walls and
newly exposed pixels invalidate via their effective inputs. A future world-space
cache needs stable placement/world coordinates and authoritative offscreen updates.
Material names/selectors are resolved per instance before the screen-pixel loop.
CPU field/mesh staging vectors retain capacity under `renderMutex` and reset
contents on every presentation. Effective field values, transformed authored
normals and mesh samples are resolved in one screen-pixel pass. Texture/buffer
uploads complete before asynchronous presentation is queued, so scratch pointers
are never retained by the GPU/presentation queue. Scene changes still clear all
fields, and each resource slot still receives its own complete upload.
Full/cache differential tests include sparse edits, ramps, domains, coverage,
structural fields, resize and quantum changes. Benchmark results are in the handoff.

`remaster-lighting-benchmark --direct` compares both paths at 256×224 with 128
mixed planar/depth emitters. On an M3 Max the initial run reduced medians from
15.36/54.15/71.93/76.79 ms to 14.23/53.33/70.43/73.92 ms in the four synthetic
scenes, including source preparation, with zero half-float output differences.
These are synthetic direct-transport measurements, not live-room FPS results;
another run after early transparent-candidate rejection measured original/prepared
medians of 13.30/12.87, 48.64/46.96, 64.11/63.50, and 66.08/68.65 ms. The mixed
results indicate that preparation alone is not a robust large speedup;
visibility remains the next optimization target. The lighting suite passes
1215 checks with production direct cases using prepared sources and accelerated
versus unprepared reference comparisons.

The live metrics panel additionally reports field-entry waits, mesh construction
(including emission-depth and normal finalization), emitter count, and authored
direct-sample count. The latter excludes the analytic debug sphere's 128 samples.
Scene fields still includes waits; its mesh submeasurement excludes them.

The Emission inspector includes **Emission Depth** (0–64 source pixels), applied
to selected tiles. Each emissive pixel has its existing XY footprint and extends
outward along its transformed authored normal. Four midpoint samples distribute
direct light over that depth, with normalized weights preserving the existing
pixel intensity calibration. Nonzero-depth samples emit in all directions,
approximating an optically thin flame rather than a one-sided surface. They still
test mesh visibility individually. Zero depth retains original planar emission.

For torches, start with depth 2–4 and a normal pointing outward from the wall.
This changes emitter geometry, not wall opacity, tile height, artwork, or visible
self-emission. The setting is tile-wide but only emissive pixels contribute.
It is copied/reset with the Emission layer and full-tile metadata, supports undo,
and can be applied to multiple selected tiles. Animation tiles need the same
setting (select together or use full-frame-to-variants copy).

Profiles store `emission_depth` under `[[assets]]` using schema 14; captures use
schema 21. Older profiles/captures default to zero and remain readable. Portable
tests cover profile/frame round trips and old-schema compatibility.

## Session debug sphere

The debug light is a spherical area emitter with independent analytic geometry;
it does not need additional scene height layers. Radius describes its physical
extent in all three axes, and height describes its center. Thus center height 15
and radius 10 occupy heights 5 through 25. Color and intensity use the same
emission calibration as authored surface emission. Enlarging the sphere increases
emitting area and softens shadows; radius is not an influence-distance cutoff.

Direct light uses 128 deterministic samples over the sphere's subtended solid
angle. Each direction intersects the sphere to obtain a full XYZ endpoint for
pixel-voxel visibility. Integration weights already include source area, facing,
and distance; receiver facing, roughness, reflectance, and visibility are applied
separately. The light emits in every direction. Receivers inside the session
sphere see an enclosing two-sided emissive boundary, bounded by the existing
transport cap, rather than a singularity or a black interior. Energy is injected
only into Direct; reflected Direct feeds the normal indirect chain.

Composite and Emission show a presentation-only circular projection whose front
surface height is `centerZ + sqrt(radius² - planarDistance²)`. Higher scene
geometry hides that glow. Contribution views exclude it. The sphere remains
session-only and is not stored in profiles or frame captures.

Right-drag moves the sphere in the scene. Holding Command during right-drag
adjusts center height instead: up raises it, down lowers it, with one source
pixel of height per source pixel of vertical travel. Height stays within 0–4096,
the panel updates immediately, and dragging preserves the enabled state.
