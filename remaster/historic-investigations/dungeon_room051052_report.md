# Room051 own052: authored walls and stair receivers

**Room051 / pinned own052: reviewable but incomplete.** Reusable runtime repairs
recover 48 wall placement bases and 256 stair scene receiver pixels. No authored
profile/array edits, room rectangle, hash elevation whitelist or commits.

## Corrected diagnosis and exact pins

`~/Library/Application Support/Snes9x/Remaster/frame_room051_052.s9xrmf` SHA256
`92d066b871607eba5f716204f762b48b3c02492012b2b5336b11432f0e0e1b6a`;
profile `81bc268905392dd39543d160f0173b125692f99aeb0bd0feb23c7d53703606b1`,
serialized FNV `9b10f9906e22e5ea`; complete ALTPDI1
`c0ae69aac1f1bac9de0fb496c25e1291ecbadbb1dfc8a83f07f91c5a8043274b`.
USA ROM `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Settled module7/submodule0, Link world(840,2874), plane0, scroll(720,2766),
BG1/BG2 graphics bases16384, rise50, independently authored stair relation24.

The `_052` pinned profile equals the current profile byte-for-byte. Compared with
`_049`, only one unrelated asset semantic entry was removed; all six contexts and
1782 bindings are identical. The east/west repeated wall bands DO have authored
0..7 ramps, matching the user's grayscale evidence. Their instance/context/flip
sources were inspected, not just hash-key presence. None of the affected wall
hashes has a context or flip/facing override. Foot966069b2157ea21f still has no
height in either pin, but the prior conclusion that wall attachment must await
new canonical foot authoring was too broad. It ignored independent tangent
contacts to supported Wall_Pillar assemblies.

## Source → solve → receiver repairs

* `frame.h`: after exact authored obstacle support resolution, a wall's complete
  nonflat one-axis local section can meet the tangent edge of an independently
  solved authored assembly. All eight transformed seam samples must agree on one
  signed base. Continue only compatible classified solid wall sections on that
  gameplay plane; do not cross missing arrays, flat edges, corners, unowned
  assemblies, nonblocking underpasses or hidden distinct BG1 art. Multiple anchor
  datums remain conflicts, never averaged. No authored local arrays are replaced.
  This recovers48 of72 unknown BG2 walls to base24 (effective24..31), including
  **560 actual scene pixels in own052**. Missing foot data was not needed for these
  independently proved tangent paths.
* `frame.h`: explicit effective StairTop/StairBottom joins Wall/Corner/Floor in
  empty cone-free scene/subscreen physical receiver recovery. Requires USA pin,
  BG1, real subscreen ownership, exact palette art, effective height+normals,
  empty main+scene and no unresolved/conflict occurrence. Both ownership and RGB
  are restored; no lamp mask/unknown-role or guessed geometry is admitted.
  c21e390677b3449f (3..5) and dd29a2e039ea282b (0..6) each recover128 actual scene
  pixels. Inspector arrays were already valid; the scene had no receivers.
  Missing scene-height pixels768→512. The residual512 are four Unknown-role
  subscreen assets, not arbitrarily accepted stair/floor geometry.
* `dungeon_authored_wall_audit.cpp` now inventories effective context/placement/
  variant sources rather than using hash-level height existence for its receiver
  denominator.
* `alttp_capture_complete_gate.py` now writes runtime structural plane claims into
  ALTPHM3 as well as the visible array; previously it manufactured48 additional
  compiled-map mismatches for newly solved nonwalkable wall cells.

## Regressions and corpus

`dungeon_wall_obstacle_seam_test.cpp`:144 combinations of both BGs, axes, H/V
transforms, exact/partially overwritten endpoints, missing/ambiguous ownership,
flat/nonaffine sections, underpasses, conflicting anchors and missing arrays.
`scene_subscreen_wall_test.cpp`: stairs join prior physical role/flip tests;
explicit context-only, placement-only and flip-variant-only signed sources prove
that hash-level absence is not absence of effective geometry. Existing priority,
HUD, malformed, context-absence, conflict and lamp-mask negatives remain.
Both narrow suites pass ASan/UBSan. `dungeon_background_ownership_test` caught an
early cross-plane leak; the rule now preserves that independent-plane gate.
The new tangent regression is in the full portable test runner.

Production EndFrame replay verifies150429 BG hash/flip/canonical-pixel observations
(150173 before stair recovery), all authored asset arrays unchanged,256 stair
receivers and560 raised wall field samples. Effective wall field audit finds
12622 authored wall pixels, zero suppressed fields or missing receiver fields.

Available complete corpus:64 Application Support room-state bundles /7 room IDs,
plus12 build bundles. Nineteen Room051 states each recover48 walls (912 placement
claims total); no formerly solved walls lost. Twelve build states unchanged.
Four newly witnessed conflicting tangent datums per affected state are explicitly
counted (runtime conflicts4→8), not hidden; existing solved outputs are preserved.
Receiver corpus126 indoor packets: five change (051044/045/046/048/052),1090
missing-height pixels recovered total, no losses or rejected reads. This is NOT
320-room/all-state coverage; missing complete sidecars/other ROM contexts remain
unsupported. Hash/role/empty inventories for both complete BGs are retained.

## Mandatory room-wide gate (not passed)

`build/room051052-repair/` contains exact pinned replay manifest/profile,
complete ALTPPD1 plus matching ALTPAT1, WRAM/PPU comparison, complete observed-seed
static movement graph, runtime JSON, ALTPHM3, v2 JSON/SVG, before/after production
packets,8192-placement inventory, profile comparison and corpus/test/build logs.
Exact collision matches own049; that graph has no invented stair playback edges.
World seed remains pinned, movement nodes are room-local.

Validation v2 exits1: supports862/886, missing24 local sources (foot9660),
plane denominators[886,0], overlap0; fallback26/26; BG2walls636solved/24unknown;
8runtime conflict records; compiled2missing/2mismatches at stair rail endpoints.
The remaining24 wall cells have authored0..7 ramps but completed attr6e and
incompatible/unsupported tangent contacts; special hookshot/support semantics and
assembly continuation need independent evidence. They are not missing wall arrays.
BG1 remains0solved/288unknown walls with existing seam conflicts. Thus neither
the reachable gate nor complete-room geometry is approved. All8192 BG placements
are inventoried: BG1nonempty missing-local17, BG2nonempty missing-local88;
BG2empty460 explicitly excluded from solved-geometry counts. Unknown-role art is
not counted as solved just because an array exists.

055 fixture rerun:439/439supports, validator exits1 (one conflict,8unknown walls).
060 rejects corner face seam disagreement;061 rejects diagonal source lacking
independent ROM proof. These are remaining gate blockers, not successful fixtures.

Native arm64 Debug build now succeeds (restart fixed the earlier environmental
SIGTRAP). Narrow normal/ASan tests and the final immutable quick gate16/16 pass;
results are in `tests-final2/20261023T045318Z-yup4ztu4/` and the final build log is
`debug-build-final2.log`. Earlier quick runs rejected in-flight edits and one
correctly caught the cross-plane leak; they are not substituted for the final
unchanged-source pass. Fresh interactive matching gameplay
across cameras/planes has NOT been performed; generated review packets are not
approval or replacement maps. Smallest next action: prove attr6e wall/assembly
ownership and the conflicting tangent datums with handler-level provenance, then
close BG1 seam/foot-source and stair-endpoint validation gaps without reauthoring
the already valid wall/stair arrays.

## Independent final verification

Parent-session full portable plus production Metal gate passes **32/32** on the
final unchanged sources, including the new wall/assembly seam regression.
Results: `build/room051052-parent-verification/20261023T045814Z-ie7xudiu/results.json`.
The arm64 Release build also succeeds; updated product:
`build/release/Snes9x.app`. Build log:
`/private/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/room051-052-release.log`.
The user's profile and all seven own052 capture inputs remain byte-identical to
the investigation baseline. `git diff --check` passes. These checks do not remove
the explicit room-v2 blockers or substitute for fresh interactive gameplay.
