# Door opening floor datum repair

## Failure and shared rule

The masked Door consumer explicitly returned zero offset for every unmarked
pixel. Room051 capture `frame_room051_024.s9xrmf` reproduces the failure at the
north door: BG2 cells (30,5), (33,5), (30,6), (33,6) have zero-height openings
beside a height-50 landing, although their frame regions already attach at 50.

`DoorframeAttachment::openingBase` now records the independently solved
room-facing landing of the same ROM door assembly. All reproduced, visible
ordinary body-support contacts along that exact landing must agree. Hidden
underpasses, unknown support and contradictory landing bases do not supply a
guess. The consumer uses the frame base for marked pixels and the landing base
for unmarked pixels. Canonical local heights remain immutable, so authored
stair/threshold relief is retained relative to its supporting floor. This is an
actual instance-height change shared by lighting, inspector and height view,
not a debug tint or Room051 exception. No profile arrays were edited.

## Evidence

Artifacts, immutable capture-profile copies, exact ROM/state/input hashes and
complete-room reports are under:

`/private/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/door-opening-floor/`

Pinned Room051 inputs replay successfully. All four north mixed frame/opening
cells now have opening base 50 (previously 0); marked heights remain unchanged.
The complete ALTPPD1/ALTPAT1 validation-v2 artifact is
`room051-validation-v2.json`. It checks placement bases, effective local arrays,
fallback directions and both planes. Door-region issues: 0. Marked placements:
8/12 solved; four missing placements are at the **south BG1 door**. Reachable
support: 76/76, plane denominators [76,0], missing support 0; structural conflicts
4 and unknown walls 24. This graph has only the captured seed, not independently
observed stair traversal; it does not establish complete gameplay coverage.
Whole-room approval is explicitly NOT granted.

Available capture corpus: 316 indoor packets; 92 settled complete inputs replayed
across 16 rooms, 223 missing complete inputs, 1 refused unsettled input. Corpus
uses the same pinned Room051 profile as a **research metadata replay**, not
capture-profile approval of every room. Visible partial-mask records: 1046;
362 acquire nonzero opening bases, 244 have unknown landing evidence and remain
unchanged. Remaining records have established zero landing bases. Unknowns need
general support/assembly ownership evidence, not zero fills or nearest floors.
Unsupported templates, unowned tiles and doors without solved frame attachments
are outside this repair's accepted region scope.

The 92-capture before/after comparison changes 7,124 visible opening samples;
all 97,308 marked frame samples and every non-region solver output are identical.
Counts include repeated capture states, not unique game geometry.

## Verification

- arm64 macOS Debug build: PASS (`build/debug/Snes9x.app`).
- New `door_opening_height_test.cpp`: PASS; signed bases -19/0/37/50/83,
  all H/V combinations, main/sub screens, hidden/conflicting plane ownership,
  unknown landing, unchanged authored heights and existing instance offsets.
- `dungeon_door_foot_test.cpp`: PASS; solver landing attachment in all four
  directions and flips, wrong assembly part/nonzero seam/special blockers.
- Door placement, signed jamb and mixed-floor tests: PASS.
- ASan/UBSan opening, foot and complete geometry-cache tests: PASS.
- Independent door-region audit test and room validator's 25 tests: PASS.
- Existing `doorframe_runtime_test.cpp` fails its first single-attachment count
  assertion (two plane records are produced); it does not reach opening checks.
  Its assertions were not weakened. New regression is separate and registered
  in the full portable test gate.
- Existing 055/060/061 complete-room fixture validation was attempted: 055 and
  060 remain non-approvable (439/439 and 711/711 reachable support, one conflict
  each); 061 refuses a diagonal preview without independent ROM proof at (2,38).
  None of these failures is hidden by a viewport-only or hash-presence claim.

Fresh interactive gameplay/lighting review remains pending. The old captured
packet itself is not rewritten; use a fresh live frame from the rebuilt app.
