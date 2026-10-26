# Room061 entrance4/default plane0 — reviewable but incomplete

## Capture014 correction — reproduced terminal-source defect (2026-10-26)

**Room061 live capture014, plane0: reviewable but incomplete. The user's
southeast defect is confirmed. The earlier 98 conditional pixel passes were
not terminal-runtime proof and are superseded by the corrected counts below.**
Detection-only: no runtime, profile, authored array or placement-map changes.

Located the exact bundle in
`~/Library/Application Support/Snes9x/Remaster/frame_room061_014.s9xrmf`.
Packet SHA256 `75b4ec0d3d8df37458266e2d7b9dc30a41762335dfb61a8c75b1fb4ff81b07f9`;
profile SHA256 `48cab744f08a15e2b6e0c1ce30ca20ad903a11152ac23d14b5492f45656b99bc`,
serialized FNV `7b2f260489f8d515`; input SHA256
`7b8261fafc5d6c837caac393c8bbfe35dc231eb4aa947526f021a59db0bb062d`.
ROM remains USA `66871d66…6009cfb`. The live profile has **rise50**, not the
earlier report's stated rise24 (the pinned profile also configures50; the
authored stair relationship solves24 independently). Live state: previous
room062, room061, module7/submodule0, epoch2333, Link(909,3348), local(397,276),
scroll(768,3240), room-local camera(256,168), plane0, collision mode0,
graphics bases16384, map bases4096/0, sizes3/3. No live entrance is attested.
The profile differs from the old pinned profile in eight asset records,
obstacle groups and preview multiplier; it is **not** the same profile.

### Exact packet versus complete-room evidence

Both complete BG maps match the pinned reference `fresh.raw` **8192/8192 words**;
both completed collision planes match byte-for-byte. The live WRAM maps also
match its complete VRAM/PPU maps. ROM template sidecar matches the pinned ROM.
The existing own-input production replay validates the serialized profile pin
and every observed BG identity, H/V transform and canonical tilePixel in all
three buffers: **168,640 samples**, no map/camera mismatch. Captured versus
current own-profile production replay: **zero BG effective-height/source-
availability differences** across those samples. Not a stale packet or a
guessed camera offset. Map/attribute equality permits reference writer/shape
inspection; it does not assert identical historical live/reference gameplay
state, graphics animation, entrance or profile.

### Concrete missing southeast band geometry

All27 surviving high SE writer30 BG2 placements fail, **1728/1728 observed
main pixels**, repeated in scene (not3456 independent physical pixels).
There are no SE high BG2 subscreen pixels. HV flips are verified against the
complete maps. These are the cap/upper/middle members already independently
proved in the earlier audit, not a new height guess:

|BG2 cell|Actual visible span|Independent expected span|Example pixel|
|---|---|---|---|
|(59,38)|1..15|25..39|screen(216,136), room(472,304), canonical63: **1 vs25**|
|(60,38)|9..23|33..47|screen(224,136), canonical63: **9 vs33**|
|(61,38)|17..24|41..48|screen(232,136), canonical63: **17 vs41**|

The same three bands continue along the transformed diagonal. All captured
high members use base0, immutable asset relief, `hasPlacementHeight=false`,
no context override; selected metadata has heights, but the **wrong effective
local source**. Low SE writer56 BG2 has1380 observed main/scene samples and
agrees with its independent conditional shape; BG1 underneath contributes1380
subscreen samples, remains an unresolved independent BG1 shape contract and
does not rescue BG2. Six trim members per relevant BG are exclusions, not
solved geometric coverage. Upper/lower gameplay planes are never collapsed.

The SW assembly is outside this east-half camera: **zero observed SW samples**.
Do not claim a SW gameplay capture comparison. Complete own-input replay and
the corrected consumer identify the same missing-source class on all27 SW
high writer21 members, e.g. BG2(2,38) actual17..24 vs expected41..48, and
(3,38)9..23 vs33..47, with V flip. Both mirrored assemblies therefore have a
complete-room source-transport defect; only SE is visually demonstrated here.

### Root cause and detector correction

`S9xRemasterSemanticDiagonalExtents` changes the effective classifier height
and intercept to a seven-band0..48 shell but preserves an earlier
`diagonalAuthoredPreserved=true` flag. Concrete SE(59,38): classifier
intercept39, min0/max48, derived25..39; immutable asset1..15. In
`frame.h`, `localHeightPreviews` excludes entries marked authored-preserved.
The54 high SW/SE placements have **zero exported local previews**, base0 and
no obstacle exclusion. Application therefore cannot transport the derived
shell into their captured occurrences. This is a **classifier-to-runtime
local-source transport/flag-consistency failure**, not a floor-base mismatch,
missing hash metadata, shape-contract disagreement or evidence for adding24
to the entire assembly.

The offline terminal detector compounded this: it preloaded its synthetic
frame metadata with `t.height` (already transformed by classification), then
used that same derived array as the fallback when application exported no
replacement. Thus it bypassed the missing transport and falsely called the
high members passes. The detection-only consumer now starts with immutable
profile metadata, retains selected flip/facing variants and obtains actual
samples via `S9xRemasterResolveFrameGeometry`. No solver outputs changed.
New read-only pixel inventory and capture/contract join check actual selected
sources independently, retain main/sub/scene ownership and reject camera,
hash, flip, pixel-index and complete-map mismatch.

Corrected three-fixture corpus (055/060/061,24576 placements, same original
profile/state pins): room061 diagonals **54fail,44pass,112unresolved,24excluded**
(previously0fail,98pass). The112 unproved bands remain44 hidden BG1 low plus68
BG2 north high. Rooms055/060 have no diagonal or supported straight-stair
dispatches in these fixtures; zero checked is not a pass for those classes.
Room061 straight-stair results remain6pass/2fail/8excluded. The source defect
class is shared across mirrored/transformed assemblies, not room/hash repairs.
No additional room/state corpus or all-room coverage is claimed.

### Gates, regressions and review artifacts

Artifacts isolated at `build/room061-capture014-audit-20261026/`:
`pins.json`, `captured-pixels.jsonl`, `packet-contract-join.json`,
`capture-vs-offline-failures.json`, `failing-placements.json`, complete
`source-replayed-sources-bg*.jsonl`, corrected three-room structure JSONs,
own-input `.s9xrmf`, solver-only ALTPPD1/ALTPAT1 wrapper, profiles and logs.
`southeast-review.png`: captured RGB / actual0..60 / conditional expected0..60 /
red mismatch, all at3x. Whole viewport original/scene, ownership and height PNGs
are included. Reviewed the crop: high-side three bands incorrectly repeat the
short intrinsic slope; expected panel continues the lower shell. No synthetic
expected panel is presented as a gameplay capture.

Own-input source regression passes and explicitly observes54 missing transports
and27 visible SE members/1728 pixels, rather than accepting solved labels.
Join tests3/3, structures7/7, doorways13/13 and validator26/26 pass. Existing
diagonal suite26/27 retains the previously reported low-cap assertion failure;
not hidden or altered. `git diff --check` passes. Runtime unchanged: no app build
or interactive post-repair capture is applicable.

**Exact live-state complete-room v2 remains blocked**: observed Link seed differs
from the recorded reference stair-route doorway seed; graph builder correctly
rejects borrowing those routes (`trace must begin at captured doorway seed`).
The failed attempt is recorded, not turned into a live gate pass. A separate
explicit **reference entrance4 graph scope** rerun uses the byte-identical live
maps/attributes, capture profile, fresh solver at reference seed248,448, complete
provenance/atlas and rise50: v2 **exit1**,1570/1570 supports plane0,0 plane1,
22/22 fallbacks,1016 solved walls/0 unknown claims, missing marked east doorway
members4 (36/40),2 runtime structural conflicts. No compiled map exists in the
capture bundle; none was substituted. This scoped gate is not proof for the live
Link state and does not prove source transport (the independent packet test is
why the false-negative was found). Room remains unapproved.

Smallest next action: authorize the shared derived-source/preserved-flag repair
and terminal transport invariant, then recheck both mirrored assemblies,
transformed/overwritten and ambiguous cases without changing authored arrays.
For exact live-state v2, acquire or reproduce stair transitions from the captured
seed, or add a separately validated graph-reseeding capability; a copied graph
with edited state is not admissible. Fresh west-camera gameplay is still needed
for the SW visual comparison. No guessed offsets or geometry repair were made.

---

Historical investigation below is retained for provenance; its diagonal terminal
pass claims are superseded by Capture014's actual-source comparison above.

2026-10-26, **detection only**. No runtime, placement map, authored arrays or
profile edits by this investigation. Concurrent profile/obstacle work is not
part of this result. Immutable profile
`025e261c0ece848ea9fb800d7d678b0bb7b23b6b9d753248be410230da62743d`, rise24;
USA ROM `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Room061 source theme4/main7/sub0, entrance4, Link248,448, collision mode0,
gameplay plane0, stair kind2. Complete snapshots/attributes/provenance and exact
state hashes are embedded in the new audit JSON. Previous artifacts preserved.

## Reporting omissions versus actual detection gaps

The prior handoff wrongly hid the south and lower-left findings as “prior
failures.” They are real **remaining defects**, not exemptions:

* **South upper doorway band BG2 `(30..33,58)`**, four members: actual16..39,
  expected0..23, all64 samples wrong per member. The intact south template
  and exact room-facing floor contacts prescribe datum0 and the band/cutout
  geometry. The previous detector already found these; failing to tell the
  user was a reporting error, not newly discovered detection.
* **Lower-left BG2 `(5,31..34)`**, four members: actual0..7, expected16..23;
  **`(6,31),(6,34)`**, two members: actual0..7, expected0..15,43 wrong samples
  per member. BG1 and BG2 must not be collapsed. Correct BG1 underneath does
  not rescue the BG2 surface actually winning compositing. These six also
  appeared in the prior detector output. Full expected/actual arrays and
  masks, not only their spans, are retained.
* Upper-left: BG2 `(3..4,16..17)` actual0 expected24, four members;
  `(3,15),(3,18)` each has one incorrect sample **despite equal24..39 spans**.
  These six were genuinely omitted by the shared one-based-writer bug.
* North exit stair BG2 `(31..32,5)` actual7..11 expected31..35, two members.
  Next tread `(31..32,6)` is27..30; landing datum24. Intrinsic tread edges7→6
  must remain a one-unit descent, not actual7→30. New reusable stair contract
  detects this without flattening the ramp or relying on reachable coverage.

All22 doorway failures remain explicitly reported, including the east six
BG2 `(56,32..33),(57,31..34)`. No runtime repair is claimed.

## Shared provenance repair and reliable corpus counts

`alttp_doorway_detection.py` now reads writer IDs1..count as `objects[id-1]`;
zero andffff are not dispatch records. Both candidate enumeration and exported
record attribution use the same convention. The new regression serializes a
real ALTPRV1 envelope, exercises **last writer == count**, and rejects a later
non-door writer of identical artwork. Existing H/V and ambiguous-template tests
remain. The old synthetic regression had accidentally encoded zero-based IDs.

|Room|Actual final door writers|Writer cells|Intact assemblies|Checked|Pass|Fail|Unsupported cells|
|---|---:|---:|---:|---:|---:|---:|---:|
|055|2|60|3|36|20|16|24|
|060|4|72|4|48|6|42|24|
|061|4|92|4|48|26|22|44|

These supersede the original report's misattributed writer/cell denominators.
Total132 checked members,52 pass,80 fail,92 unsupported writer cells. Unsupported
is neither a geometry failure nor a pass. Detection exits1 for all three.

## Diagonal investigation: concrete geometry, unresolved user symptom

The previous “no additional effective defect established” wording was too broad:
it did not establish an exhaustive independent pixel contract for every solved
wall. `alttp_structure_detection.py` now checks final-dispatch ownership and
independent **transformed per-pixel geometry**, including solved walls, using
the existing independent diagonal-proof engine. It enumerates unsupported bands
instead of treating metadata presence, solver status or equal min/max as proof.

**The reported southwest/southeast partial-height symptom was not reproduced in
the pinned terminal consumer. It remains an open user-reported defect, not an
automated pass for the live scene.** No evidence-backed incorrect pixel or
nonzero corrective offset can be asserted for those pinned shapes. Adding24
would break their lower-floor0 contacts. Exact observations:

* SW high writer21, handler0x0e, origin(2,34),10 repetitions; surviving BG2
  cap `(x,x+36)` for x2..11, upper `(x,x+35)` for x3..11, middle
  `(x,x+34)` for x4..11. All27 agree with independent full-shell proof.
* SE high writer30, handler0x10, origin(52,43),10 repetitions; surviving BG2
  cap `(x,99-x)` for x52..61, upper `(x,98-x)` for x52..60, middle
  `(x,97-x)` for x52..59. All27 agree.
* SW low writer45, handler0x1a, origin(5,34),7 repetitions;22 surviving
  geometric BG2 members. SE low writer56, handler0x1c, origin(52,40),7
  repetitions;22 members. All44 agree. Six extra trim members are explicitly
  non-wall exclusions, not solved geometry. Identical BG1 low shells have44
  geometric members but lack the necessary independent same-layer shape proof;
  they remain unresolved, and have zero visible pixels in the six pinned PPU
  viewports. Hidden geometry is not counted as a visible defect.

The proof uses a clipped canonical shell `clamp(intercept-u-v,0,48)` with seven
actual bands, not a repeated four-band cap. SW is V-flipped: canonical
`(u,v)=(px,7-py)`, displayed slope `-px+py`. SE is HV-flipped:
`(u,v)=(7-px,7-py)`, displayed slope `px+py`. Transforms apply to masks too.

Examples (room-local BG2, px/py0..7; **actual = conditional expected**):

|Placement|Displayed terminal / expected geometry|Span|Sloping-region mask|
|---|---|---|---|
|(2,38)|`min(48,48-px+py)`|41..48|`0080c0e0f0f8fcfe`|
|(3,38)|`40-px+py`|33..47|`ffffffffffffffff`|
|(4,38)|`32-px+py`|25..39|`ffffffffffffffff`|
|(5,38)|`24-px+py`|17..31|`ffffffffffffffff`|
|(8,38)|`max(0,py-px)`|0..7|`7f3f1f0f07030100`|
|(59,38)|`25+px+py`|25..39|`ffffffffffffffff`|

The cap mask excludes its48 plateau; the foot mask excludes its0 floor region.
All displayed arrays and masks are in `room-061-structures.json`. Support is
same-layer Floor actual-minus-authored datum0, preserving0..1 edge relief:
SW contacts `(8,37),(9,38),(10,39),(11,40),(12,41)`; SE mirror contacts
`(55,37),(54,38),(53,39),(52,40),(51,41)`. No nearest floor or hidden BG1 datum24
was borrowed. Conditional contacts are observations, not absolute anchors.
The visibility join retains all64 sample indices and each member's actual
visible-pixel count from the prior six matching complete-map PPU packets.

**Coverage:** Room061234 final-owned diagonal dispatch members:98 conditional
pixel passes,112 unresolved shape proofs,24 non-wall exclusions,0 pixel failures.
Unresolved112 =44 hidden BG1 low members +68 BG2 north high members. This is
NOT a complete diagonal all-state proof. No room rectangle or hash list drives
the detector. Lack of proof is an explicit detector/evidence gap.

Smallest action for the user's south-diagonal symptom: obtain a matching fresh
Snes9x frame/profile/state showing the incorrect region and compare its terminal
samples/selected local source against these coordinates and masks. Current
evidence cannot distinguish a live-source/state difference from a missing
contract for another diagonal region. Do not dismiss the symptom or invent
expected heights from its description.

## New stair scope and regressions

Object-created straight inter-room stairs handlers1e..21 use exact ROM Nx4
templates11a8/11c8/11e8/1208 and one surviving final writer. All16 words must
survive; partial/identical-art overwrites remain unresolved. Two exact exterior
landing contacts on that BG establish a consistent contextual datum. Only
authored Floor members participate in the floor-datum contract; rail/wall parts
are excluded pending a region contract. Room061 one intact BG2 assembly:
8 Floor members checked,6 pass,2 fail;8 other members excluded. Rooms055/060
have no supported straight-stair dispatches and no diagonal dispatches in these
states; “zero checked” is not a geometry pass.

New7 structure tests cover H/V/HV partial masks and equal-range pixel failure,
relief-preserving datums and conflicting/other-BG contacts, absent shape proof,
ambiguous ownership, incomplete8192 inventory, intact/overwritten stair members.
An integrated partial-wall regression changes only one sloping pixel while the
floor region stays correct, so a solved/unchanged-span label cannot hide it.
Doorway13/13 and complete-room validator26/26 regressions pass. Existing diagonal
suite26/27 passes: `test_low_authored_bands_and_full_wall_direction` fails on
the immutable pinned profile as well as the working profile (expects a low cap
clipped24 but actual starts31). Not altered or hidden by this diagnostic work.

## Whole-room gate, scope and artifacts

Complete-room v2 rerun retained at
`build/room061-followup-audit-20261026/validation-v2.json/.svg`: **exit1**, not
approved.1570 support cells plane0,0 plane1;22 fallback placements;1016 solved
walls/0 unknown wall claims;2 structural conflicts;48 compiled-map mismatches,
22 map-missing entries. Same unchanged solver/map as the previous pinned audit;
these representation disagreements are not automatically visible geometry
defects. No new gameplay comparison. No two-plane overlap in Room061; detector
does not merge layers/planes, and Room060 doorway checks retain split ownership.

Actually audited automatically: three pinned default/entrance fixtures055/060/061,
24,576 BG placements consumed by each detector, not317 additional rooms or
alternate states. Room061 full8192 visibility ledger reused only with its exact
snapshot/profile/state pins. Other available generated discovery corpora were
not claimed as terminal/gameplay coverage. New output directory preserves the
old20261025 audit and records input digests, fresh terminal-consumer invocations,
per-rule counts, expected/actual64 samples, masks, support contacts, unsupported
reasons and a visibility join. No runtime code changed; no app rebuild required.

Reproduce the new read-only contract with the same `--snapshot`, `--attributes`,
`--state`, `--profile`, `--rom`, `--consumer`, `--output` arguments as the doorway
detector, replacing its script with `alttp_structure_detection.py` and adding
`--atlas build/remaster-geometry/alttp-atlas.sqlite`. Build the terminal consumer
with `c++ -std=c++17 -O2 remaster/tools/ALttP/alttp_terminal_height_inventory.cpp`.
Both tools deliberately return1 for known failures/unproved required contracts.

Next reusable capability: independent shape/support ownership for the112
unproved diagonal bands, region-specific rail/stair contracts, and a matching
gameplay terminal-source comparison. Unknowns are blockers to remove, not a
reason to label this a successful room or production-ready geometry.
