# room061 fresh-capture regression (2026-10-02)

Verdict: **reviewable but incomplete**, not room approval.

Inputs: `frame_room061_002.s9xrmf`, frame schema 25, ROM SHA256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`;
complete `surface-audit-room-local/room-061-door-input.raw` ALTPPD1 and
ALTPAT1, doorway graph and state (entrance 4, Link 248,448, plane 0).
Capture height metadata matched all 121 shared assets in
`alttp-profile-reference-diagonal-v2.toml`; the dirty root differed at five
height arrays when inspected. A packet does not record the profile filename,
so this is metadata equivalence, not proof of which path was selected.
No root/backup/profile normals were edited by this fix.

## Causes and changes

* `gfx.cpp`: room-local projection used raw PPU VOffset; DrawBackground uses
  LineData VOffset = PPU VOffset + 1. The missing scanline bias assigned the
  first displayed row to the previous room cell, split instances, and made
  alternating black/white stripes. Use the identical biased origin.
* `remaster/frame.h`: diagonal classification now recognizes exact clipped
  affine profiles; full cardinal seam samples remain eligible for diagonal
  assemblies. Downhill wall-to-wall extension samples the receiving edge
  slope, making cap/face transitions reciprocal. Point-only diagonal contacts
  cannot bypass an intervening authored cardinal placement. Compatible wall
  tangent seams now extend solved bases to wall pieces without their own
  direct floor contact. Conflicts still invalidate components, and negative
  bases/unknowns are not clamped or filled. Plane support and visible walls
  stay separate; no nonblocking underpass wall is treated as walkable.
* No shader change: Height debug/Metal already use canonical tilePixel and
  placement-local height (if present) OR metadata, plus offset once. The
  reported wall/diagonal instances have no placementHeight replacement.

## Exact before/after regression

After values below are recomputed packet application over the matching full
room fixture, **not a newly captured app execution**.

| Screen sample | Before | After |
|---|---:|---:|
| south wall main (181,100), original instance2103 | 8 | 8 |
| same sub instance407 (BG1) | 0 | 0 |
| south wall main top row (176,93) | 23 | 15 |
| west-of-stairs main (224,96), original instance1453 | 4 | 12 |
| same sub instance413 (BG1) | 4 | 4 |
| bottom31aba (72,69), first row | 34 | 30 |
| same bottom (72,70), next row | 9 | 29 |
| middle3bc2 (64,69), first row | 14 | 38 |
| same middle (64,70), next row | 37 | 37 |
| upper503a (56,69) | 13 | 45 |

The connected south face has base8, span8..15 at both reported occurrences.
The main IDs can merge differently in fresh frames; original IDs above are
only the supplied capture's identifiers. BG1 remains unmodified, not copied
from BG2 merely because hashes agree.

## Reproduce portable checks

Compile `remaster/dungeon_capture_height_test.cpp` with C++17, pthread, both
`-O0 -g -DNDEBUG` and `-O2 -DNDEBUG`. Pass capture, candidate, complete raw,
and raw.attr. It verifies the exact reported instance pairs, matched profile
metadata, placed identity, and row continuity on a 512 KiB worker stack.
`remaster/dungeon_geometry_test.cpp` additionally checks diagonal clipped
seams under all four H/V transformations and wall tangent attachment.
Existing small-stack tests pass at 512/544 KiB. Validator tests: 8 passed;
reference-template tests: 18 passed. Broad remaster_test stops at its existing
hard-coded root asset-group count (expects11), incompatible with the shared
dirty root; do not alter the root or weaken that test to conceal it.

Fixture runner now also emits ALTPHM2 and passes it to mandatory v2 validation:

| Room | supports | plane denominators | walls solved/unknown | conflicts | exit |
|---|---|---|---|---|---|
| 055 | 439/439 | 439,0 | 986/232 | 1 | 1 |
| 060 | 711/711 | 437,274 | 343/146 | 2 | 1 |
| 061 | 1570/1570 | 1570,0 | 662/88 | 1 | 1 |

All have zero missing support. The v2 JSON/SVG effective-height inventories,
runtime JSON and compiled binary are in the preapproved temporary directory's
`capture-fix-after/`, not authoritative source. Room061 remaining issue is an
isolated authored-flat graph with 84 negative-base nodes, no equality
mismatches. Cap a4aa still lacks a compatible anchored region: its irregular
authored seam is not an exact continuation of generated upper503a. Preserve
it unresolved; fixing this requires regional cap/assembly ownership, not
changing its user-authored arrays or assigning the nearest floor.

No publication or capture handoff approval: gates remain nonzero. Parent
must build the macOS app because runtime code changed; no macOS build was
performed here. Fresh gameplay review after the remaining gate failures are
resolved is still required across cameras/stairs/planes.

## Follow-up: exact cap-region investigation

No additional runtime change was made. `tools/alttp_cap_contact_probe.py`
checks complete final-map ALTPRV1 equality, complete ALTPAT1 room identity,
placed flips, same-writer contacts and authored cap/face samples independently.
It inventories **all40 a4aa placements**, not just the capture crop.
`test_alttp_cap_contact_probe.py` locks the actual contradictory seam and
verifies that splitting cap/face, or reversing displayed sample order, cannot
turn inconsistent face votes into a solved contact.

At room-local `(10,5)`, a4aa belongs to writer20. Its east `(11,5)` and south
`(10,6)` neighbors are upper503a, also writer20, both base32. Both contacts
produce identical evidence:

```
displayed a4aa edge:       5  5  4  3  2  1  1  0
displayed upper503a edge: 13 13 12 11 10  9  8  7
cap mask at edge:          C  C  F  F  F  F  F  F
required a4aa base:       40 40 41 41 41 41 40 40
```

Cap contacts compare horizontal samples directly. Face contacts use the
receiving face's exact one-unit boundary slope, as the runtime does. Thus the
cap region has a compatible relative constraint `base=a4aa_neighbor_base+8`,
but **the face region itself has conflicting +9/+8 requirements**. This is not
fixable by a single cap/face pair of offsets while preserving authored ramps.
Choosing base40 leaves four face samples one unit low; choosing base41 leaves
the last two one unit high. Without extrapolation, cap wants+8 and face
wants+8/+7, still inconsistent. It is not an ambiguity in flip sampling.

The adjacent north/west c405 placements are collision1, local zero and writer111
(not20), or another layout writer. They are **not reachable ground**. The
runtime's isolated-flat datum sets their component to0, producing cap base-5
and tall-wall bases-48 etc. This is the actual origin of the84 negative-base
nodes, not84 contradictory equality edges: that component has no support
anchor. Do not shift it upward merely to make unsigned bases fit.

All40 caps belong to writers20/21/30/31 (dispatch phase1, source offsets
20/23/50/53); their same-writer neighbors are upper503a. Room061 unresolved
wall inventory is a4aa40, 62112d42, 37192c2, 921eab2, ccf87d2 =88. The negative
component accounts for84 (caps40, tall walls42, 37192c2); remaining four have no
solved ownership/support. Regional cap-only solving would also need a way to
retain unresolved face pixels independently—current byte placement maps cannot
claim complete wall coverage from a solved subset.

Smallest required authoring decision: confirm whether a4aa's four low face
samples versus its last two samples are **intentional relief/discontinuities**,
or whether the upper503a generated continuation is the wrong local profile.
If intentional, supply an explicit seam/region policy that preserves that
relief (including its cap-to-face internal seam); if exact continuity is
intended, authorize a distinct height candidate correcting the conflicting
samples. No normal changes are needed or proposed. Separately, c405 needs
explicit excluded/cap/assembly ownership rather than floor-group-as-ground.

Evidence artifact: temporary `cap-regional-review.json` contains input hashes,
all40 cap placements, transformed edge samples, all contacts, writer records,
and the complete88-wall unresolved inventory. It is a review artifact, not a
new runtime map. Fixture runner now pins one byte-identical profile copy for
all three rooms and validators, writes its SHA256 manifest, and reports source
changes during the run. This prevents shared candidate regeneration from
mixing solver and validator inputs. Revalidated pinned inputs: same full-room
counts above, all exits1. Three regional regression tests pass.
