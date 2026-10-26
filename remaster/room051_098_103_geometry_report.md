# Room051 captures098/099/103 geometry

## Parent integration verification

Parent independently passes ASan/UBSan lower-writer capture098 transforms/overwrite
proof, capture099 EndFrame replay and capture103 follower-source replay. Logs and
replayed packets are under `build/room051-098-103-parent/`. Independent Python
lower-writer/doorway/complete-room-validator tests pass28/28. arm64 Release and
Debug app builds pass. Inspector25checks, tooling14tests and mesh39sanitizer
checks pass. The fixture-free NPC support test is registered in the curated full
suite; final aggregate result is recorded separately.

Parent reran all three recorded complete-room v2 commands: each remains exit1;
outputs are retained in `validation-{098,099,103}.log` in the parent evidence
directory. This does not waive structural/map mismatches or establish gameplay
approval. The reported103 crossing remains unreproduced, not declared fixed.
Parent independently checked all256before/after BG corpus rows (128input names):
ownership1924→2992, attachments432→544, visible conflicts104→0 and required
unknown doorway denominator1848→2448. Structural conflicts608, unknown walls1488,
stair members2624 and zero missing supports remain unchanged. The expanded unknown
doorway denominator is not silently counted as successful geometry coverage.
Final stable curated CPU/Metal gate passes **44/44**:
`build/room051-098-103-parent/full-tests/20261026T021929Z-6hwdo2sp/results.json`.
Independent exact071 replay retains288solved BG1 walls and16stair members with
unchanged arrays. Canonical authored profile remains byte-identical to the parent
pre-change snapshot; final `git diff --check` passes. No fresh interactive visual
acceptance, successful complete-room gate or Zelda-crossing reproduction is claimed.

Room `0x051`, own captures098/099/103, Module7/submodule0:
**reviewable but incomplete**. Two shared production geometry defects are repaired.
The follower consumer now uses its producer contact bounds and delayed plane,
but the reported Zelda discontinuity is **not reproduced in exact103**. Every
complete-room validation-v2 command still exits1; this is not room approval or
a handoff for approved gameplay capture.

## Exact inputs and preservation

Inputs are each packet's own companions under
`~/Library/Application Support/Snes9x/Remaster/frame_room051_NNN.s9xrmf*`.
ROM SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`;
own profile SHA256 `025e261c0ece848ea9fb800d7d678b0bb7b23b6b9d753248be410230da62743d`,
serialized FNV `a0a42bc961c35a5e`. The canonical profile independently has that
same SHA256; it was NOT substituted for the three own profiles or edited.
All own companion SHA256 pins are in
`build/room051_098_103/evidence-summary.json` and each gate manifest.

| own packet | packet SHA256 | Link XY | scroll XY | exact BG observations |
|--|--|--|--|--:|
|098|`56295ee6f5f87638b6bbca4891e1b0926a7d009db70e14ea4d9e5c6c953338ae`|859,2987|739,2833|149902|
|099|`277fcb80e86e7f01d95b672b07451a612c20f22217e8862dd6acda1e22421be3`|761,2644|641,2561|166066|
|103|`bd6de379ef485fd02c1bf5e7ad41f0154a5870e66d9359969663890a627dce07`|761,2665|641,2561|166092|

All have active Link plane0, previous room061, graphics bases16384, rise50.
Complete WRAM and PPU maps agree; template companions equal the fingerprinted ROM.
Complete inputs are respectively `2e71d4ab2d2c9c623bb9c15152dbc05d9f5cfe6d9e86d07615e18ac4e460a08b`,
`318946221c88f918df53dd63ded737c9bfed8e4e5269ffe1841f618787e73eec`,
`7dbd8f7f48382563c665fbf36ba4a7874cef43849ad443828baab02bd676801f`.
Original packets/companions remain unchanged. All initial concurrent work is
preserved. No macOS/profile/runner edits, commits, aggregate suites or app builds
were made by this worker. Parent owns aggregate/build integration.

## 1. North099: hidden support was conflated with visible frame ownership

The reported BG2 support placements `(30,6),(33,6)` really are authored Doorway
`482a1674f4994ba7`, local0..7, canonical support south. Their exact ordinary
template ownership is intact. BG2 floor `(30,7),(33,7)` is `8ac2095165d05a61`,
flat local0, reproduced plane0/base24 and plane1/base0. Both formerly voted
**visible**, so application rejected conflicting24/0 and returned base0.

The BG1 cells at the same coordinates are DIFFERENT authored floor patterns,
not duplicate doorway owners: row6 `d2ef88b0a91300d6`/`f105c9a9889035dc`,
row7 `4d5247551edca28e`/`9c8a1e10362481e4`. Their distinct solved support
must survive without voting a second visible frame datum.

Source `Dungeon_LoadBasicAttribute_full` writes0x1000 pairs by indexing from
`dung_bg2` through both contiguous maps: BG2 into gameplay array0, BG1 into1.
The shared support rule uses that writer relationship **only when both final
map identities are distinct authored zero floors at the explicit contact**.
It preserves both plane attachments but marks the other-map contact hidden.
Equal words and ambiguous/non-floor ownership retain the old rejection contract.
No active-Link selection, camera visibility, nearest/highest floor, artwork
similarity or closed-Door classification is used.

After: north exact assembly retains plane0 opening24/frame bands24/32/40,
and hidden plane1 opening0. Own099 physical scene has398 affected doorway pixel
uses overall; `(30,6)`64pixels and `(33,6)`16observed pixels now use base24,
unchanged local arrays/normals. Its main-screen stock references remain base0:
EndFrame applies physical geometry to the independent scene, not to main's
untouched presentation ownership. BG2 row7 physical scene remains base24;
BG1 floor24 is not copied onto a BG2 doorway or used as actor-plane evidence.

## 2. South098: exact row56 verified; missing handler, not overwritten cap

The user's exact `(30..33,56,BG1)` was inspected without silently changing it:

| row / layer | placements / local source | result |
|--|--|--|
|56 BG1|30/33 `9eb947628ab00ab3`, Doorway0..7|base0 unchanged|
|56 BG1|31/32 `9c5f860181cb54fb`, authored Floor **-8..-2**|signed relief unchanged|
|57 BG1|30/33 `c72c0d2c90678917`, mixed Doorway0..7|frame8/opening0 unchanged|
|57 BG1|31/32 `fda59404d2049e61`, Floor0|unchanged|
|56/57 BG2|final tile01ec, not doorway words|no invented structure|
|58 BOTH|30/33 `369fb6b4b3f569f8`,31/32 `b789b1b314f9abd6`, Doorway0..7|base0 →16|

The four cap tiles are row58, not four cap cells at row56. More importantly,
the previous092 report's assumption that row58 was an overwritten ordinary
three-word door was incomplete. Inspection of ALL final words finds exact
subtype2 lower inter-room stair handler29, template offset`0x1288`, four-word
columns at origin `(30,55)`. Handler28/29 writes all16 BG1 cells and its terminal
row to BOTH BGs. Row55's roomward trim and row56/57 signed opening words match,
as do all four shared terminal words. This is a full writer proof, not a new
partial-column guess or same-coordinate inheritance.

Production recognizes the complete handler26..29 family, all H/V transforms,
all required words on both layers; priority-only writes are harmless. A changed
opening or either shared terminal word rejects full ownership. Alternate local
selectors cannot supply proof. Complete claims are collected before publication;
different claims remain unowned. The existing directional floor0 on plane1
attaches the BG1 assembly; the exact shared terminal relation transports its
datum to BG2. Row56/57 remain0/8, row58 becomes16+local0..7 on BOTH actual writes.
Authored mixed masks/opening relief, including negative heights, remain intact.

Own098 changed512 observations:256 BG1 sub and256 BG2 physical-scene cap pixels.
BG2 main's stock refs remain0. Row56 is not raised merely because the user called
it the top. No whole ordinary frame was fabricated from the old partial proof.
The092 partial-column regression now explicitly overwrites a signed opening word
to isolate that independent fallback; intact092 uses the newly proved writer
and still retains467 restored stair and256 inner-jamb physical scene pixels.

## 3. Zelda103: producer contact repaired; reported crossing not present

The actual103 packet already has EVERY OAM68 head and OAM69 body record at24,
including fully occluded scanline records. No per-tile base24/0 split exists.
Head children32/33/48/49 have local10..25, body34/35/50/51 local0..15,
all with authored normals and no context or placement override. Representative
hashes are `afa3d89315a9d741`,`adcd84116adc9842`,`c5eaec4c6182d2a1` (head),
`19e81163b6f815da`,`90a05d85e9566090`,`2f1c89745c386ba3`,`91eee0b2f92faac8`
(body). Full instance/OAM/bounds records are in `actor-san.log`.

Both main AND physical scene show head x121..134,y87..99; body x120..135,y97..107.
With pinned scroll641,2561 these lie around room columns31..33, rows11..13.
Reported BG2 `(31,17)` maps to screenx119..126,y135..142: it is NOT under Zelda
in exact103. Its `833d6d68ba42b851` is authored Floor local0, no context override,
with reproduced plane0 support24, lower0; it is not an actor local-height source.
Local head/body heights and normals remain exactly unchanged after replay.

Nevertheless the shared consumer did use clipped visible ink to choose contact.
It now uses the attested lower16x16 OAM body bounds, independent of transparency,
occlusion and scanline splitting. The exact follower producer's `Tagalong_Draw`
signature also supplies its delayed movement-history plane through
`kTagalongFlags[0]=20,[1]=10`; this is decoded ONLY after the complete two-OBJ,
DMA-child/palette/flips/relative-pose attestation, never for generic OAM priority.
It no longer borrows current Link plane for an independently delayed follower.
Hidden sentinel/unsupported producer states remain unsupported. Blocked relief
cannot vote and contradictory/unknown floor contacts reject support. No local
BG relief or nearest floor is pasted onto the sprite.

Exact103 before/after follower base stays24 and local/normal sources byte-equal.
This is a reusable contact/plane repair with synthetic clipping/independent-plane
proof, NOT proof that the user's reported103 visual symptom is fixed. A fresh
capture at the actual crossing and matching Height view is the smallest remaining
evidence action; do not alter approved actor arrays to force an unobserved result.

## Corpus and targeted regressions

Union of all available complete Application Support own inputs and retained092
corpus:128 unique own-input combinations,256 BG solves; rooms002(4),011(1),012(6),
022(1),041(2),051(89),061(25), no missing companions/basic-input rejections in
this union. List and exact pins: `corpus-inputs.json`; before/after CSVs retain
8192 BG placements per pair, including unknown/empty/non-geometric entries.
Not all320 rooms, not all states, not every viewport or NPC pose.

| shared solver metric | before | after |
|--|--:|--:|
|complete-frame owned cells|1924|2992|
|directional plane attachments|432|544|
|unknown doorway cells|1848|2448|
|conflicting visible doorway cells|104|0|
|structural conflicts|608|608|
|unknown walls|1488|1488|
|attached stair cells|2624|2624|
|required supports plane0 /1|269772 /688336|unchanged|
|missing supports plane0 /1|0 /0|0 /0|

Unknown doorway count INCREASES600 because newly recognized complete structural
members become required in older profiles lacking directional anchors. This is
an exposed defect denominator, not solved geometry.178 BG audit rows change;
new ownership does not authorize guessing heights in those historical contexts.
Actual viewport scene source inventory is exact098/099/103, not all256 solves.

Passing focused tests: new exact lower-writer test (all H/V, split maps, priority,
signed-opening/terminal overwrite rejection); doorway propagation (all axes/flips,
split ownership, distinct hidden support, active-plane independence); NPC contact
(transparent/clipped torso overlap, independent delayed planes, contradictions,
immutable local arrays), exact103 OAM/local/normal replay; surviving columns,
door-foot, stair-assembly support, Link attachment, scene subscreen ownership.
New lower-writer Python audit test passes; independent doorway propagation test
and26 complete-room-validator tests pass. Exact071 retains288 BG1walls,16stair
members, rejected BG2 overwrite and immutable authored arrays; exact061010 retains
896 effective doorway pixels/169178 identity observations. Exact092 retains
467stair/256jamb scene pixels. New/changed focused tests and all three exact
EndFrame replays pass ASan/UBSan; repeated unchanged-profile resume packets and
complete maps are byte-identical. `git diff --check` passes.

## Mandatory complete-room validation and blockers

Each `gate-{098,099,103}/gate.json` records the full validator command, SHA pins
and actual exit1, using complete ALTPPD1/ALTPAT1, exact pinned state, own generated
profile, linked doorway graph preserving transferred reference directed edges
only on exact collision match, runtime solution and compiled ALTPHM3 map.
Validation JSON/SVG and before-gates are retained beside them.

All three doorway gates improve from0/20 accepted (16ambiguous-visible records,
4missing intact template records) to **20/20, zero doorway issues**. Whole-room
reachable denominators098 `[886,0]`,099/103 `[76,0]`; all reachable supports valid,
zero unknown walls, passing independent stair audit. These graph denominators
are NOT runtime's larger candidate counts `[962,3720]`, nor plane1 gameplay review.

Residuals: all three have4 runtime structural conflicts.098 compiled map misses
BG2 `(30,39),(33,39)` expected24 (two missing/two plane mismatches);099/103's
compiled map has four north-foot mismatches (visible0 vs expected24, plane0
unknown vs24). The compiled gate map still projects ordinary support instead
of mask-scoped doorway attachments; it cannot be declared corrected by changing
the report denominator. Runtime physical-scene north attachments DO apply24,
as exact source inventory and doorway probe demonstrate. General compiled-map
region representation/projection and the separate stair-row provenance gap remain
blockers. No fresh interactive gameplay/occlusion comparison was performed.

Smallest next actions: repair compiled support/structural region representation
without flattening masks or planes, resolve four structural conflicts/upper stair
row source, rerun v2 to zero, then inspect fresh gameplay across cameras/planes.
For issue3 obtain the actual Zelda crossing packet; current103 is not that contact.
