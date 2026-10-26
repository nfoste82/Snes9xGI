# Room061 doorway propagation

Room `0x061`, settled Module7/submodule0, own `frame_room061_010`:
**reviewable but incomplete**. The reusable runtime repair is implemented and
the two reported frames now compose their structural band rise with the proven
landing datum. Complete-room v2 still fails an independent diagonal-source proof;
this is not a successful-room or capture-approval handoff.

## Exact reproduction and diagnosis

Inputs remain untouched under
`~/Library/Application Support/Snes9x/Remaster/frame_room061_010.s9xrmf*`.
ROM SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`;
own and current profile SHA256 both
`025e261c0ece848ea9fb800d7d678b0bb7b23b6b9d753248be410230da62743d`;
serialized FNV1a64 `a0a42bc961c35a5e`. No profile edits were made. All pre-existing
dirty changes were retained; no macOS source or runner files were edited.
Link `(652,3126)`, plane0, scroll `(532,3073)`, BG graphics bases both16384,
configured rise50, previous room051. Complete maps/VRAM/templates and own profile
pass the exact production EndFrame replay, matching169178 BG pixel usages and
their H/V transforms. The solver-wrapper export also has zero WRAM/PPU map
mismatches; template bytes equal the fingerprinted ROM.

The initially suspected membership gate is **not the reported capture's primary
failure**: every member already has an authored nonzero mask and unique complete
ROM assembly ownership. Before the repair, all eight north and all eight west
members already had plane0 attachments at24. The old solver transported the
landing datum but never added the handler-relative band rise. Thus support feet
looked right while middle/outer bands stayed too low. Exact north ownership is
origin `(30,4)` / assembly287 / stair-frame template0x11e8; northwest/west is
origin `(2,15)` / assembly963 / ordinary west template0x2f42 (H-equivalent source
template recognition). Both support directions are explicitly authored and
placement-transformed. No height was inferred from the adjacent wall, BG index,
visibility, room number, or hash.

## Shared runtime rule

An intact uniquely owned three-band ROM frame carries one **landing datum** per
gameplay plane. Every legitimate Doorway member with local height may transport
it, regardless of whether its frame mask is absent, full, partial, or explicitly
empty. A mask describes pixel regions, not membership or connectivity. Remote
jambs/caps of the same proved writer footprint therefore do not need an invented
mask bridge. Different origins/directions/sources and ambiguous or closed Door
members do not transport support.

The displayed handler axis/origin determines band0/1/2 from the roomward end.
Frame pixels use `landing + 8*band + authored_local`; opening pixels use
`landing + authored_local`. Authored signed relief and masks are unchanged.
Absent masks on proved structural members mean full frame; an explicit empty
mask remains opening-only. No floor/support node or neighboring wall is added.
Split-BG frames transport an explicit assembly/source/direction/plane landing
relation from the independent BG1 solve, not the other BG's visible base.
Conflicting landing claims invalidate the assembly on that plane; visible
cross-plane disagreement still withholds application.

The independent Python doorway audit now reproduces complete ordinary/stair
structural footprints, transforms, split routing, competing ownership, landing
contacts and band offsets. It no longer requires opening artwork to match a
structural writer or equates every frame-band base with the opening base.

## Entire reported frames: before/after

Coordinates are room-local 8x8 cells, **BG2 only**; these frames have no recognized
BG1 members. All local authored spans are0..7 and opening datum24. Plane1 is
unobserved/unattached, not filled. `effective after` includes authored opening
pixels on mixed parts. Before every listed placement had frame base24 and
effective24..31 in the actual geometry scene buffer.

|BG|X|Y|support|frame base before→after|effective after|
|--|--|--|--|--|--|
|2|30|4|no|24→40|40..47|
|2|31|4|no|24→40|40..47|
|2|32|4|no|24→40|40..47|
|2|33|4|no|24→40|40..47|
|2|30|5|no|24→32|24..39|
|2|33|5|no|24→32|24..39|
|2|30|6|yes|24→24|24..31|
|2|33|6|yes|24→24|24..31|
|2|2|15|no|24→40|40..47|
|2|3|15|no|24→32|24..39|
|2|4|15|yes|24→24|24..31|
|2|2|16|no|24→40|40..47|
|2|2|17|no|24→40|40..47|
|2|2|18|no|24→40|40..47|
|2|3|18|no|24→32|24..39|
|2|4|18|yes|24→24|24..31|

For these two frames only, the user's wall reference independently agrees:
north neighboring wall `(29,4/5/6)` bases40/32/24, west neighboring wall
`(2/3/4,14)` bases40/32/24 (the clipped outer local cap is0..5).
Those wall outputs are unchanged and were **not solver inputs** to the repair.
The complete per-placement masks/ownership/planes and effective observations are
in `build/room061-doorway/{before,after}.doorways.csv`, `*.pixels.csv`,
`after-effective.jsonl`, and `two-frames.json`. Raw `map.offsets` may remain255
for Doorway because mask-specific attachments, not tile-wide offsets, drive
runtime geometry. This distinction is explicit in the CSV.

EndFrame's unoccluded scene buffer is the actual geometry consumer when present;
main remains stock presentation. Counting unraised stock main pixels as missing
geometry would be erroneous. The probe checks896 effective doorway scene pixels
against every visible attachment, preserving local arrays and rejecting guessed
placement-height replacements. Repeated unchanged-profile publication/resume
produces byte-identical packets and complete-map inventories, including the new
split-support relations.

## Corpus and regression evidence

`build/room061-doorway/corpus-inputs.json` enumerates all locally available
complete own-input packets discovered in Application Support and build:133
bundles,116 unique packet/profile/state/input combinations,232 BG solves,
rooms002/011/012/022/041/051/061. All116 original serialized profile pins match;
zero missing companions or rejected basic inputs. These are sampled repeated
states, **not320 rooms or all room states**. Neither per-view visibility nor
duplicate captures establish complete game coverage.

Same-field before/after corpus inventories and per-plane attachment records:
`all-{before,after}.csv`, `all-*-attachments.csv`, `corpus-summary.json`.
2708 doorway cells;1828 recognized frame cells;0 ambiguous frame cells;
44 support markers;168 plane attachments retained,126 frame bases corrected,
0 lost and0 added attachment claims. Structural conflicts560, unknown walls1512,
152 stair attachments/2432 stair members, and required support denominators
246684/599056 remain unchanged, with zero solver-missing supports. These summed
denominators include repeated room states; they are not unique map cells.
Unknown doorway count1836→1864 because the repaired denominator additionally
accounts for28 recognized non-mask structural members; no new solved claim is
fabricated. Zero doorway conflicts. Unmarked art remains explicitly unsupported.

Passing focused tests: new fixture-free `dungeon_doorway_propagation_test`
(all axes/flips, disconnected nonmask caps, explicit-empty parts, signed local
relief, split BG, competing/ambiguous ownership and actual closed Door negatives),
also ASan/UBSan; frame-recognition288 cases (full footprint overwrites on either
BG and competing origins); door-foot, doorframe-runtime, doorframe-placement,
door-band, door-jamb, opening, owned-wall projection, semantic-cache and
placement-reuse tests. Exact Room051071 regression matches147451 usages and
preserves16 staircase members and288 solved BG1 walls. Python validator26 tests
and new8 transformed/split independent doorway-audit cases pass. `git diff
--check` passes. Parent owns runner registration, aggregate gates and app builds;
none were launched here.

## Complete-room gate and residual blockers

Both before and after exact own-profile complete-room v2 commands are retained
in `gate-before/gate.json` and `gate/gate.json`. The generated complete ALTPPD1,
matching ALTPAT1, full linked doorway graph, pinned state, own profile, runtime
solution and compiled ALTPHM3 map are supplied. Reference collision SHA matches
exactly; directed stair edges are retained. Reachable denominator `[1570,0]`.
**Both commands exit2**, before producing a validation JSON/SVG, on the same
independent pre-existing error:
`directional diagonal preview source lacks independent ROM proof:
layer=1 cell=(5,38)`. It was not suppressed or stripped to make a gate pass.
Current profile equals own profile byte-for-byte, so there is no distinct
current-profile research substitution to run.

The separate complete-room doorway audit has40 required Doorway placements,
24 solved,16 explicitly missing (the two unmarked lower east/west frames),
zero accepted-record issues. It is diagnostic evidence, **not a replacement for
the failing room-wide gate**. Actual production room maps still have BG1 six
unknown walls, BG2 two structural conflicts, zero missing reachable support on
either BG, and160/160 authored obstacle members attached. Every8192 BG placement
is retained by the complete-map/diagnostic inventory, but empty/transparent and
unclassified artwork are not certified as solved geometry.

No fresh interactive gameplay capture or visual acceptance was performed. Do
not hand this room back as capture-approved or publishable while v2 is nonzero.
Smallest next shared action: extend the independent diagonal-source proof for
the overwritten southwest handler class, then rerun the exact complete-room
gate; subsequently inspect fresh matching gameplay at both reported frames and
relevant cameras/planes. Missing support authoring on other doors and remaining
BG1 structural ownership remain separate coverage defects, not grounds for
neighbor-wall inference or default-height fills.

## Independent parent verification

Curated full CPU/Metal gate passes **42/42**:
`build/room061-doorway-parent/full-tests/20261024T051834Z-1xe9oozc/results.json`.
arm64 Release and Debug builds pass; logs under
`build/room061-doorway-parent/{release,debug}-build.log`.
Independent ASan/UBSan exact010 replay reproduces169178matched BG usages and
checks896effective scene doorway pixels. Authored profile matches the parent's
saved pre-repair snapshot byte-for-byte. Inspector25checks, tooling14tests and
mesh39sanitizer checks pass.

Parent independently reran31Python doorway/validator tests successfully (the
first combined invocation timed out at120seconds; the300second rerun completed
in143seconds). The historical frame-audit fixture now supplies explicit landing
evidence and handler-relative outer band16; it rejects missing landing evidence
and flattened offsets. No runtime/validator proof was relaxed for that fixture.

The recorded complete-room v2 command was independently rerun: exit2 on the
same BG2(5,38)diagonal-proof failure, retained in
`build/room061-doorway-parent/validation-v2.log`. No room approval, fresh gameplay
visual review or authored support changes are claimed.

## Parent independent verification / Python fixture follow-up

Parent reports exact010 ASan replay passes169178 usages/896 effective scene
pixels, and arm64 Release/Debug builds pass. Parent owns the running aggregate
CPU/Metal gate; no aggregate gate was launched here.

The historical `test_doorframe_runtime_audit` fixture previously accepted an
arbitrary frame-only base with no proven opening/landing datum and later equated
the outer frame band with that datum. It now supplies explicit plane landing
records and independently correct two-band rise16, including a negative hidden
plane datum. Added negative assertions reject both missing support evidence and
flattening the outer frame base to the landing. This follow-up changes only the
Python fixture and this report: no runtime header or validator proof was weakened.

From `remaster/tools/ALttP`, the combined command
`python3 -B -m unittest test_doorframe_runtime_audit test_alttp_validate_room_heights test_doorway_metadata test_doorway_propagation_audit`
passes31 tests (`build/room061-doorway/python-parent-followup.log`). The new
transformed/split audit module is
`remaster/tools/ALttP/test_doorway_propagation_audit.py`: one unittest method
exercises all8 flip×split cases, including wrong-band and overwritten-word
rejection. This is distinct from the C++ propagation/recognition regressions.
