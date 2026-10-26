# Doorway detection only — 055/060/061 reviewable but incomplete

**2026-10-26 correction:** the original denominators below used ALTPRV1 IDs as
zero-based and are superseded by the [Room061 follow-up](room061_detection_followup_report.md).
Shared indexing is now fixed and tested with a serialized last-writer packet.
Reliable055/060/061 checked/pass/fail/unsupported counts are36/20/16/24,
48/6/42/24 and48/26/22/44. The south Room061 BG2 `(30..33,58)` and lower-left
BG2 `(5,31..34),(6,31),(6,34)` defects were already detected; omitting them from
the handoff was a reporting error. They remain defects, not “prior” exemptions.
The follow-up also inventories diagonal partial pixel contracts and the north
inter-room stair failure without any runtime/profile edits.

2026-10-25. No runtime, profile, metadata arrays, or height maps changed. This is
a scalable executable contract audit, not an LLM coordinate whitelist or a height
repair. A nonzero detection result is expected on the current fixtures.

## Room060 corrections to the earlier audit

The user's review supersedes the earlier wording in
`build/room060-detection-audit-20261025/REPORT.md`:

* Middle-east BG1 **(56,31) and (56,34) are correct**. An ordinary base of 255
  is not proof of an effective-height defect; their terminal samples are 0..7.
  The new independent contract checks both as **pass**.
* The middle-east outer band is duplicated on BG1/BG2. Correct BG1 at x58,
  y31..34 is hidden by wrong BG2. Explicit split ownership requires BG2 to
  derive from BG1, not a maximum of the two gameplay-plane solutions. Expected
  BG2 samples are 16..23; actual 31..38. Interior BG1 x56..57,y32..33 is 16,
  versus its contiguous outside supporting datum 0.
* North/top BG2 x46..49,y7 is wrong, not merely an unapproved seam suspicion.
  The BG1 counterpart and explicit BG1 supporting floor imply 0..23 with
  cutouts, versus actual 16..39. Interior BG1 x47..48,y8..9 is 16, expected 0.
* Bottom and upper-east findings remain confirmed. Interior floors expected24
  are respectively 0 and52. Closed/locked **Door** usages are structural members,
  never included in the open-floor equality just because collision permits it.
* Additional user-reviewed **correct wall geometry**: BG2 x46..49,y4..6
  (12 cells) and x59..61,y31..34 (12 cells), **24 cells total**. These are not
  missing-height defects; this is user review, not a new automated check.
* BG2 x42..53,y39..44 (**72 cells**) has missing data, but the user reports
  that the player sees BG1 there and that BG1's data is correct. The BG2 absence
  is therefore **likely intentional**, not a confirmed missing required height.
  Correct BG1 is user-reviewed; compositing ownership and whether BG2 geometry
  is required remain unsupported by the automated detector.

These additional reviews do not change the detector outputs or counts below.
The detector has not verified these new facts: user-reviewed correct geometry
and likely intentional layer absence must remain separate from automated passes.

## Reusable detection and evidence

`tools/ALttP/alttp_doorway_detection.py` enumerates supported 3x4/4x3 ROM door
templates on both complete final BG maps, including split dispatch and H/V flips.
It requires **all twelve surviving words** and one actual final door writer from
ALTPRV1. It does not trust the runtime's `door_assembly`, `opening_base`, claimed
frame attachments, or visible maximum-plane base. Equivalent templates collapse;
competing ownership and overwritten templates cannot lend ownership. A wall-only
template match does not become a doorway merely because its words match.

The owning layer is BG1 for a proved split dispatch. Two exact room-facing
opening contacts determine the external floor datum, not a proximity search.
Both must have Floor metadata and matching terminal-minus-canonical offsets.
The open 2x2 interior with flat Floor metadata must equal that datum. Door metadata
is exempt from this floor rule, but retains its authored relief under the
assembly rule. Canonical profiles are loaded read-only; transformed masks and
local arrays are checked in displayed coordinates. Frame bands are handler-local
8-unit relationships; already-encoded cap bands are not added twice. Split BG2
cap samples use the adjusted BG1 counterpart when it is classified Wall/Corner,
retaining explicitly identified zero cutouts.

The external floor and BG1 counterparts are terminal observations, **not independent
absolute elevation anchors**. Thus expectations are independent of the doorway
solver's claims, but conditional on those recorded external surfaces being right.
Agreement is contract consistency, not proof of the whole room. Detailed closed
panels without complete supported dispatch, region geometry beyond simple ramp
blocks, geometry selectors, partial overwrites, and missing ownership stay
unsupported. No guessed floor, plane, room rectangle, hash whitelist, or authored
array modification is used to hide a denominator.

`tools/ALttP/alttp_terminal_height_inventory.cpp` runs the production terminal
height application on all8192 placements, with actual fixture room/seed/plane,
stairs, both BG solves, configured profile rise, and a SHA-pinned USA ROM. It
creates independent tiny receiver frames, **not Snes9x gameplay captures**.
The Python audit invokes this consumer afresh (no stale terminal JSON accepted),
checks tile identities against the snapshot and pins all inputs including the
consumer binary. Each checked placement records expected/actual64 samples,
mismatch pixel indices, canonical local source, support contacts and their
observed arrays, writer/ROM-template evidence and BG1 counterpart samples.

## Corpus actually checked

Same immutable canonical profile snapshot as the Room060 investigation, SHA256
`025e261c0ece848ea9fb800d7d678b0bb7b23b6b9d753248be410230da62743d`;
ROM SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Complete renderer-v21 fixtures in
`build/remaster-geometry/surface-audit-room-local/`, one pinned entrance/default
state each, 24,576 BG placements total. All other rooms/states are **not checked**;
these three fixtures do not establish game-wide or all-state coverage.

| Room | Door writers | Final door-writer cells | Intact assemblies | Members checked | Pass | Fail | Unsupported writer cells |
|---|---:|---:|---:|---:|---:|---:|---:|
|055|3|96|3|36|20|16|60|
|060|5|144|4|48|6|42|96|
|061|4|125|3|36|20|16|89|

Failures by class: 055/061 each12 doorway assembly +4 open interior;
06016 doorway assembly +10 split BG1 assembly +16 open interior. Zero unresolved
**checked** members in these runs; the separate245 unsupported final-writer cells
are not solved or passed. They include writes outside the supported twelve-cell
frame, corridor/outer art, overwritten or otherwise unproved template cells.
No door state was inferred from a filename; output embeds exact traversal state
and input digests. JSON artifacts: `build/doorway-detection-20261025/room-*.json`.
All three detection commands exit1, not approval.

## Reproduce / tests / remaining gates

From repository root:

```sh
OUT=build/doorway-detection-20261025
mkdir -p "$OUT"
c++ -std=c++17 -O2 remaster/tools/ALttP/alttp_terminal_height_inventory.cpp \
  -o "$OUT/terminal-consumer"
for ROOM in 055 060 061; do
  RAW=build/remaster-geometry/surface-audit-room-local/room-$ROOM-door-input.raw
  python3 -B remaster/tools/ALttP/alttp_doorway_detection.py \
    --snapshot "$RAW" --attributes "$RAW.attr" --state "$RAW.state.json" \
    --profile build/room060-detection-audit-20261025/pinned-canonical-profile.toml \
    --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc \
    --consumer "$OUT/terminal-consumer" --output "$OUT/room-$ROOM.json"
done
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_alttp_doorway_detection.py'
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_alttp_validate_room_heights.py'
git diff --check
```

The standalone audit deliberately does not weaken or replace validation v2.
Detection regressions **12/12 pass**: split/non-split positives, interior
self-consistency negative, Door exemption with relief, conflicting contacts,
plane overlap, adjusted BG1 counterpart, cap double-addition, H/V transformations,
north/south contacts, missing evidence, competing ownership and final-writer
overwrite rejection. Existing validator regressions **26/26 pass**.
Re-ran Room060's complete graph/snapshot/attributes/state/profile/runtime/map/
ROM/provenance/atlas validator, output
`build/doorway-detection-20261025/room-060-validation-v2.json`: **exit1** as before.
No room is successful or being handed back for capture approval. No runtime
source changed, so no app build required. No fresh gameplay comparison performed.

Next shared capability: export ordered door dispatch/part/copy provenance,
explicit support plane and counterpart ownership, including overwritten and
closed states; extend the same scripted contract audit to those unsupported
classes and enumerate the full room-state corpus. Runtime repairs require a
separate authorization and full-room gates. Do not flatten authored relief to
make this detection report green.

For the additional coverage examples, the next reusable detection capability is
to inspect decoded BG2 pixel opacity and actual compositing ownership, then
validate the effective exposed BG1 height source. Distinguish absence of geometry
from missing required heights; missing BG2 data does **not** universally imply
that BG1 is exposed. No coordinate whitelist or geometry repair is introduced
by these documentation corrections.
