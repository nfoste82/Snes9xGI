# Selected-occurrence obstacle layout and supporting end

## Behavior

The inspector now draws the saved footprint transformed to the exact selected
scene occurrence. Selecting the west horizontal pillar shows head left/contact
right; selecting the H-mirrored east pillar shows contact left/head right.
Dropdown coordinates and footprint button positions use the displayed layout;
button tags retain original member indexes so Z/contact edits affect the correct
member rather than another occurrence of the same identity. A status line says
whether the view follows selected locations or is the saved layout. Empty,
stale or ambiguous location evidence does not invent an orientation. Creation
already preserved exact selected coordinates and tile flips; that stays intact.

No profile schema or authored arrays changed. The runtime now derives a cardinal
support end only when marked contacts form a single outer row/column spanning
the noncontact body's transverse extent. Sparse/ambiguous contact layouts retain
the legacy south convention; no centroid/nearest-floor heuristic. H/V transforms
mirror the direction with the matched footprint. A horizontal right-foot group
has the same east contact direction under a V-symmetric match, so these formerly
competing claims are semantically equivalent. H mirroring changes east→west.
Both own-foot support and outward support stay plane/component/base-qualified.

`authoredObstacleDirection` is retained in placements, geometry equality and
entry-state keys. Independent Python obstacle matching/contact validation uses
the same layout-derived direction. Existing vertical, sparse, contact-edit,
signed-Z and ownership contracts remain covered.

## Pinned Room051 own039 evidence

Complete capture inputs/profile remain read-only. Production EndFrame matches
92,826camera/identity/flip samples. All64Wall_Pillar members now have one matched
assembly and base24, west direction east/right and east direction west/left.
The previous16stair-side pillar members remain24,464actual pixel usages checked;
11,953upper-floor pixel usages remain24. No configured stair fallback returns.

Mandatory validation v2 in `build/oriented-obstacle-validation/` uses complete
matching ALTPPD1/ALTPAT1 and captured state. Exact source pins are in gate.json:
ROM SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`,
profile FNV1a64 `3b98a54cc03c23ea`, profile SHA256
`370b3d0d58edf65b61a506b5560d49b2642b9dd24e2928e78e82618a9339abb3`.
Authored obstacles80→144solved /144required, ambiguous64→0, missing0;
contacts28/28→44/44witnessed. These are complete placement/contact counts,
not hash-level height presence or viewport coverage.

Room gate STILL exits1: reachable denominator[0,360],360/360valid; fallback4/4,
0conflicts; BG2walls564solved/54unknown;4structural-conflict nodes;
compiled-map4missing/54mismatches; doorframes8/12. BG1 lower-area graph still
has419bad nodes/288unresolved walls from the independently recorded authored
seam discrepancy. This task fixes horizontal obstacle ownership, not those
remaining structural walls or the entire room. No stair traversal invented.
Full floor bases/local sources/effective ranges, fallback directions, plane
denominators and missing counts remain in the artifact.

## General regressions / corpus

* `obstacle_group_editor_test.cpp`: both occurrence orientations/all four flips,
  mirrored displayed contacts, stable member IDs, existing sparse/ambiguity,
  undo/roundtrip/save-merge coverage.
* `dungeon_oriented_obstacle_test.cpp`: horizontal four-by-two group, independent
  H/V transforms, V-symmetric ownership, correct outward edge, reject missing
  true outward support. Included in full portable gate.
* Independent Python horizontal-foot regression all four flips;14obstacle tests.
*26/26portable tests,26validator Python tests,6runner tests,17inspector source
  layout tests, optimized and ASan/UBSan editor/runtime cases passed.
*arm64 Debug/Release app builds and `git diff --check` passed.

Corpus346contexts/319room IDs, both BGs/planes, unchanged own039profile;
complete cached/full-map/diagnostic/packet comparison passed.
Solved authored obstacle members1696→1840 (144recovered): Room05180→144,
Room0e0 64→144. No losses. Unmatched10272→10128;
authored obstacle conflict count2unchanged; structural-conflict sum17983 and
unknown-wall sum262701 unchanged. Default/entrance scope plus synthetic animation
cache stress is not all-state gameplay approval.
Artifact `build/oriented-obstacle-corpus.json`; full test manifest
`build/oriented-obstacle-tests/20261022T203509Z-zv_fqyv5/results.json`.

Reproduction: compile/run `remaster/dungeon_room051_height_probe.cpp` against
`frame_room051_039.s9xrmf`, then use the replay tool and sprite_obstacle_gate.py
as in `dungeon_room051039_report.md`, output to the orientation directory.
Fresh interactive AppKit selection/Undo/Save and live visual acceptance were not
performed. Relaunch `build/release/Snes9x.app` or Debug for that check.
