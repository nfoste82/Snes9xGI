# Castle-first overworld height generation

Status: implementation plan, 2026-10-15. Runtime overworld height generation
has not been implemented or validated by this investigation.

## Goal and first fixture

Generate contextual elevations and local surface geometry for the complete
castle exterior, starting with the ground, elevated terrace, connecting walls,
and obstacle immediately west of Link. Build reusable overworld rules that can
then cover other structures, ledges, and terrain.

Reference capture:
`~/Library/Application Support/Snes9x/Remaster/frame_overworld_x0771_y0690.s9xrmf`.
The supplied screenshot and user identify the blue surface as Link's upper
terrace and the green courtyard as lower ground. These are fixture expectations;
color is not a generator input for assigning elevation.

Read-only inspection with `tools/remaster_frame_summary.cpp` established:

- The packet decodes as schema 35, 256x224, with 2,124 tile instances.
- The captured profile ROM SHA-256 is
  `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
- All 57,344 main-screen pixels have linked tile instances. The summary reports
  no supported-owner/missing-instance defects.
- All visible background entries have placement offset zero and effective
  height zero. Sprite entries contain local relief but have offset zero too.

This establishes a usable visible reference and the missing outdoor elevation
behavior. It does not establish a complete castle map, collision topology,
wall projection ratio, numeric terrace rise, or pinned progression state. The
filename's coordinates alone do not provide that state.

## Core model: elevation, artwork projection, and detail

Separate three inputs:

1. **Support elevation:** the ground is a chosen datum `0`; the terrace is
   `H_castle` above it. An obstacle supported by the terrace has base `H_castle`.
   Further wall tops, parapets, towers, and roofs can have additional elevations;
   two walkable levels do not imply only two heights in the scene.
2. **Assembly projection:** face/cap masks and orientation describe how artwork
   pixels traverse an assembly's low and high endpoints. Outdoor facings can
   have unequal projected spans for the same elevation change.
3. **Local detail:** bevels, battlements, trim, and actor silhouettes add relief
   and normals relative to their established support or structural surface.

For a wall face, the conceptual rule is:

```text
z(pixel) = z_low + progress(assembly, facing, pixel) * (z_high - z_low)
           + detail_relief(pixel)
```

`progress` is normalized over the complete face, not restarted in each 8x8
tile. Its direction and span come from assembly geometry. Corners reconcile
adjacent faces to shared endpoints; cap pixels are not accidentally treated as
another wall ramp. Obstacle relief is added once to its owning base.

For example, if two faces depict the same rise over 16 and 32 artwork pixels,
their height-per-pixel rates differ by two. Those numbers illustrate the rule;
they are not measurements of this castle.

The apparent lower outdoor viewing angle motivates a separate projection
policy. A single photographically exact camera model should be tested against
the artwork rather than assumed: stylized faces can need family-specific
projection templates. Name directions explicitly as face normals versus wall
run directions to avoid ambiguous north/south-wall terminology.

The existing `camera_direction` affects shading's view vector, and
`coordinate_scale` globally scales lighting Z. Neither encodes these per-face
artwork mappings. Start by compiling outdoor geometry into existing signed
placement offsets and placement-local height layers. Validate normals alongside
height: a compressed artwork ramp's derived normal need not be an appropriate
structural-wall normal.

## Implementation sequence

### Prerequisite: resolve shared-artwork semantics per placement

The user reports that `v1:4bpp:b3881650e2d5d461` is shared plain-color
artwork used on both castle walls and the blue terrace. It is the first
required contextual-semantics regression. A content hash identifies decoded
artwork, not its structural purpose. Leave its canonical role unspecified or
context-dependent; resolve each occurrence from the final metatile/assembly
provenance, member position, contacts, and support evidence. Palette can help
identify an assembly variant but cannot independently establish wall/floor role.
If a Map16 definition is itself reused ambiguously, resolve at the enclosing
Map32/assembly level and final placement context. Do not flood-fill blank pixels
across structural boundaries or assign the nearest recognizable tile's role.

For the same hash, contextual results must support:

| Occurrence | Resolved semantics | Effective geometry |
| --- | --- | --- |
| Terrace interior | Floor, terrace component | Flat at `H_castle` |
| Wall-face member | Wall, owning face and facing | Assembly-relative progression between endpoints |
| Insufficient evidence | Explicitly unresolved | Reported missing geometry, not an inferred flat floor |

Existing placement offsets/local heights can express distinct geometry, but
existing frame roles are asset-level, and arbitrary placement-local normal
arrays are not available. The profile's `context_dependent` flag alone does not
provide end-to-end contextual classification or suppress all authored geometry.
Implement a placement-semantic result before outdoor classification/solving:

- Key it by pinned map/state, layer, cell/support surface, and provenance;
  include role, face/assembly ownership, evidence, and resolution status.
- Carry effective semantics onto frame instances. Distinguish absent overrides
  from explicitly unresolved results so ambiguous placements cannot silently
  inherit a hash-global role. Preserve these fields through instance splitting.
- Reuse placement-local heights and provide contextual normals where required,
  with explicit pixel-index and flip/vector transformation rules.
- Use this same result in classification, support solving, mesh structural
  policy, lighting, Tile Types, and occurrence-specific inspector readouts.
  Mesh behavior currently uses material/group classification rather than tile
  role alone; transport must include that consumer, too.
- Version capture serialization and invalidate relevant caches so replay and
  live presentation agree. Asset metadata editing stays distinct from resolved
  occurrence semantics.

Acceptance requires simultaneous wall, floor, and unresolved occurrences of
the same hash, including identical flips, to preserve distinct role, height,
normal, and mesh behavior across capture round-trip without altering shared
asset metadata. This is a prerequisite for the castle generator, not a later
authoring workaround.

### 1. Export a reproducible complete overworld input

Extend the existing ROM-backed overworld decoder,
`tools/ALttP/alttp_atlas_overworld.py`, which already retains
Map32 -> Map16 -> Map8 placement identities. Add a runtime/state adapter that
records overlays and later writers, final map words, collision behavior,
graphics identity, and transforms. Base ROM quadrants currently omit gameplay
overlays and changes.

Pin ROM, world, area and covered quadrants, graphics context, progression/event
state, profile/metadata version, scroll mapping, and provenance. Export the
complete castle-containing area/state and necessary neighboring quadrants.
Inventory relevant BG layers and semantic support surfaces independently of
which pixels win the viewport.

Compare expanded words and decoded tile identities against the reference and
adjacent camera captures. Capture Link on the courtyard, terrace, and actual
connecting route, including transition states. Existing nearby overworld
captures are candidates for comparison, not automatically matching fixtures.

**Deliverable:** pinned complete-area input plus matching viewport references;
explicit map/state mismatches and unavailable evidence.

### 2. Classify outdoor assemblies and solve support elevations

Add an overworld-specific placement context and surface graph. Reuse generic
constraint machinery where appropriate, while keeping outdoor extraction and
classification separate from dungeon templates, fixed room dimensions, and
two collision planes.

- Identify ground/terrace components, wall faces, caps, corners, parapets,
  transitions, and obstacle assemblies from metatile provenance and metadata.
- Connect components through supported transition/structural constraints.
  Collision and movement validate traversability and support ownership; they
  do not by themselves provide a numeric elevation.
- Establish the ground datum and terrace ordering. Calibrate `H_castle` from
  matched structural endpoints and artwork, recording any authored scale choice.
  Do not inherit the dungeon's conventional floor rise.
- Resolve the western obstacle's support through its assembly footprint and
  contacts. Its screen-space proximity to the courtyard is insufficient.
- Permit more than two levels and multiple support surfaces at one coordinate.
  Keep disconnected or conflicting assignments explicit.

**Deliverable:** inspectable support/assembly graph with evidence for each base,
transition, and wall endpoint; no castle-coordinate elevation rectangles.

### 3. Calibrate and compile the castle wall family

Annotate reusable metatile/assembly families with face regions, foot/cap
contacts, facing, transforms, and normalized height progression. Fit the
orientation-dependent spans to matching examples around the castle. Solve
faces and corners together so the terrace does not change height at a turn.

Separate walls spanning ground-to-terrace from obstacles/parapets rising above
the terrace. Preserve existing authored detail and define clearly whether each
local layer is replacement geometry or additive detail to prevent double relief.
Recognize partial overwrites and reject ambiguous assembly ownership.

**Deliverable:** generated placement bases and signed local geometry, with
per-placement source and effective height, for the complete declared castle scope.

### 4. Apply outdoors at runtime and in replay

Integrate the outdoor adapter at the frame lifecycle in `gfx.cpp` and placement
application in `remaster/remaster.h` / `remaster/frame.h`. Use canonical area/map
coordinates and each BG's actual PPU mapping, including scrolling/wrapping.
Grounded Link, actors, and attached emitters inherit their resolved support base;
airborne displacement remains separate from surface elevation.

Cache by geometry-relevant area/state, provenance, graphics, and metadata
revision. Scrolling must preserve bases; overlays, transitions, and world/state
changes must invalidate affected results. Ensure captures retain effective
geometry so offline replay reproduces the live frame.

The current mesh uses screen XY plus height. Review castle corners and wall
occlusion under several light directions to determine whether that representation
is sufficient. If correct transport requires undoing vertical artwork displacement
or representing hidden overlapping surfaces, introduce an explicit world-to-art
projection and world-space mesh positions. That follow-up must update normals,
emitters, actor positions, ray geometry, cache keys, and capture representation
together. A shading camera setting alone cannot supply this capability.

**Deliverable:** stable live/replay heights and support inheritance across the
castle traversal, with any transport-representation limitation demonstrated.

### 5. Validate the complete castle, then expand by failure class

Build an overworld complete-area validator using the existing surface-rule/audit
approach. Give the outdoor export its own versioned contract; ALTPPD1/ALTPAT1 are
dungeon formats. Report:

- Denominators by area/state, BG, role, and support surface, including offscreen
  placements; transparent, empty, and non-geometric cells separately.
- Placement base, owning component/assembly, local geometry source, transforms,
  fallback ramp/direction, effective heights, and explicit missing/conflict counts.
- Projection endpoint/corner agreement, actor support, and cross-viewport identity
  and height consistency. Mere hash-level height presence is insufficient.

Required failure-class regressions: unequal face spans with equal rise; corners
and valid flips; repeated artwork on different bases; terrace-supported obstacles;
partial overwrites; ambiguous ownership; scrolling/quadrant seams; overlay/state
invalidation; multiple overlapping supports when introduced.

Castle acceptance requires zero missing/conflicting required geometry in the
declared complete-area scope, matching live traversal and replay, and sensible
height/normal/occlusion results. Any dungeon solver changes additionally require
the existing complete-room validation v2 with matching ALTPPD1, ALTPAT1, and state.

Audit every available outdoor area/state using the supported families next.
Use the 160 base quadrants as an inventory seed, with overlays and state variants
listed separately. Group failures into metadata, provenance, classification,
support, solve, or projection classes; extend shared rules and rerun the corpus.
Record checked/unsupported denominators and the general capability needed for
remaining gaps. Subsequent families should include ordinary ledges, buildings,
bridges/underpasses, and long mountain transitions.

## Recommended first implementation milestone

Placement-semantic transport, complete-area export, and a support graph identifying the courtyard, terrace,
their connecting route, and the western obstacle, followed by one calibrated
castle wall family covering its distinct facings and corners. This produces a
reviewable foundation for automatic outdoor coverage and an early test of the
current lighting-space representation.
