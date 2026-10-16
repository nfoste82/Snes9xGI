# Castle overworld review implementation

## Open the candidate

Build the arm64 Debug application, then double-click:

`build/remaster-geometry/overworld-castle-review/Open Castle Review.command`

Alternatively choose **View > Open Area Review…** in
`build/debug/Snes9x.app` and select the bundle's `area-review.json`.
Pause emulation before opening replay. No ROM is needed for offline review.

The native panel lists three original Snes9x captures with generated contextual
geometry. Previous/Next retains the selected GI view and debug light. Use Scene
Controls for Height, Normals, Occlusion, Visibility, and direct/indirect lighting.
**Open complete-area overview** opens a zoomable browser view with occurrence
inspection, missing-data masks, issue markers, and issue JSON export. Overview
artwork is diagnostic grayscale, not captured RGB. Review packets bypass profile
metadata synchronization, so loading them does not replace the proposal with
hash-global geometry or edit the profile.

## Actual status

This is a **reviewable incomplete candidate**, not completed first-milestone
coverage or an area marked Ready. The complete 1,024×1,024 base-map area contains
16,384 Map8 placements: 6,988 solved, 576 classified without supported geometry,
8,820 unknown, zero conflicts. Its architecture-bank inventory is explicitly
separate from complete-area and raster-visible denominators.

In the supplied `x0771_y0690` capture, 52,311 of 55,199 selected-background pixels
have effective generated geometry (94.8%). Of the rest, 24 have a map/graphics
identity mismatch. The architecture inventory intersecting this viewport has
865/865 cells solved. This count alone does not prove correct geometry; v4's
review regressions additionally check the reported A–G conditions and seams.
Other BGs, runtime state variants, full-area geometry, and detailed foliage are
not established by these numbers. Existing HUD/other-layer geometry is inherited.

The candidate's authored scale is ground 0 / terrace 32, side face span16,
south face span32. These are explicit calibration choices for visual review.
Columns, crests, mixed caps/faces, and the western recess have local contextual
geometry. Fine relief and assembly interfaces still require review and repair.

Link receives base32 only in the original reference capture, using its SHA-pinned
user-attested standing fixture, explicit sole samples, and one matching support.
Other packets do not infer grounded state or actor elevation from a sprite bbox.
No new authored occlusion arrays are generated: inspect geometry blocking with
Visibility/shadows; the overview reports absent occlusion data explicitly.

## Implementation

- `remaster/overworld_geometry.h`: collision behavior, exact assembly discovery,
  support constraints, anisotropic/sample-based geometry and transformed normals.
- `tools/ALttP/alttp_overworld_geometry.py`, `alttp_overworld_castle.py`: complete
  base-area export, reusable semantic families, corpus inventory, strict gate.
- Frame schema36: occurrence status/role/normals/occlusion in addition to existing
  placement height/base; unresolved/conflict prevents shared-asset fallback.
- `alttp_overworld_replay.cpp/.py`: exact occurrence matching, ambiguous-origin
  rejection, canonical array transfer, pinned standing-fixture support.
- `alttp_overworld_review.py`, `alttp_overworld_review_bundle.py`: overview,
  annotations, native bundle and launcher.
- `macosx/remaster-overworld-review-methods.h`: lightweight native navigation.
- `remaster/overworld_capture.h` / `gfx.cpp`: first-completed-frame outdoor WRAM,
  VRAM, ROM definitions, PPU and profile evidence sidecars for future state parity.

## Reproduce

Compile `tools/ALttP/alttp_overworld_core.cpp` and
`tools/ALttP/alttp_overworld_replay.cpp` as standalone C++17 tools.
Then run:

```sh
python3 tools/ALttP/alttp_overworld_geometry.py --castle --core CORE_BINARY \
  --output build/remaster-geometry/overworld-castle-authored-v1.json \
  --write-rules build/remaster-geometry/overworld-castle-rules-v1.json \
  --review-summary build/remaster-geometry/overworld-castle-authored-v1-review.json \
  --audit-corpus
python3 tools/ALttP/alttp_overworld_art.py --area 0x1b \
  --prefix build/remaster-geometry/castle-authored-v4 \
  --candidate build/remaster-geometry/overworld-castle-authored-v1.json
python3 tools/ALttP/alttp_overworld_review_bundle.py --producer REPLAY_BINARY \
  --standing-fixture build/remaster-geometry/castle-reference-standing-fixture.json \
  --artwork build/remaster-geometry/castle-authored-v4-overview.png \
  --capture-candidate build/remaster-geometry/castle-authored-v4-open0771.json \
  ORIGINAL_CAPTURE_0771 ORIGINAL_CAPTURE_0770 ORIGINAL_CAPTURE_07C8
```

Generated ROM-derived inputs/captures stay under ignored `build/`.

## Completion gaps

The complete-area publication gate deliberately fails: missing geometry,
unobserved layers/support surfaces, unpinned gameplay state, and incomplete
matching capture coverage. The corpus audit inventories 112 area heads / 160
base quadrants / 655,360 placements; it does not approve their geometry.

Next shared capabilities: remaining castle assembly families; final-state snapshot import and
graphics parity; complete-area runtime capture traversal; runtime activation
with state-aware invalidation. Automatic live height generation is not enabled.
The new Debug build writes outdoor evidence sidecars when a frame capture is
requested while emulation runs. Existing paused replay captures cannot recover
missing WRAM retrospectively. Ten available local freeze slots were all indoors.

Portable geometry/occurrence/replay and compatibility regressions pass; the
macOS arm64 Debug build passes. Interactive GUI/lighting approval remains for
the user's review. No dungeon geometry was generated or approved by this work.

## A–G user review repairs (v4)

The labelled original RGB establishes the true lower C–D shoulder at Map8
`(234,218)` and upper F–G at `(240,198)`. An earlier audit had incorrectly
labelled the upper decorated strip C–D; that label and golden were corrected.

Shared failure classes repaired:

- Recess face progression is shared across its full width, with side/south
  corner transitions at A–B/B–C.
- Corner families reuse relative geometry across bases. The true approved C–D
  golden preserves all 20 placements; F–G matches it at +32.
- Projecting decorations no longer erase the underlying facade gradient.
  Closed doors follow their owning wall face, instead of adding a support storey.
- Caps remain geometric surfaces but are not movement supports. Terrace obstacle
  tops have +6 relief, and all 20 named logo occurrences are flat at32.
- E's open aperture is recognized from observed final words and assigned floor32.
  `alttp_overworld_capture_state.py` exports/re-verifies capture-SHA-pinned state
  evidence and actual ROM collision attributes. Only original0771 uses this
  partial-state candidate; the other captures do not inherit unobserved state.
- Upper walls/ledge use explicit stacked support relationships. Exact interface
  tests prevent corner-coordinate restarts and repeated upper-wall ramp segments.

Independent checks of the regenerated packets found zero transfer errors and
verified the four previously discontinuous interface pairs now match at32 or64.
Selected-background solved/missing raster counts:

| Capture | Solved | Missing/unresolved |
| --- | ---: | ---: |
| 0771 | 52,311 | 2,888 |
| 0770 | 51,564 | 3,416 |
| 07c8 | 53,976 | 576 |

Original RGB and raster ownership remain unchanged. Heights and normals are
occurrence-local. Corpus audit covers112 area heads/160 base quadrants/655,360
placements, with only supported castle families activated; runtime variants and
overlapping planes remain an evidence gap. The strict complete-area gate still
fails, as intended. No blanket geometry approval is implied.

Artifacts under `build/remaster-geometry/`: `castle-authored-v4-issue-audit.json`,
`castle-authored-v4-final-audit.json`, `castle-v4-label-map.png`, and the updated
`overworld-castle-review/` bundle. The review title includes the v4 revision.

Verification: seven architectural review regressions, six geometry contracts,
capture-state tamper/SHA/provenance tests, replay/standing and review UI tests.
Interactive appearance approval remains outstanding.
