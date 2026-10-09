# Room062 / immutable016 — reviewable but incomplete

## Final narrowing follow-up — supersedes initial corpus/support/south results below

Parent verification completed: independently passed terrain lifecycle, door-band,
pot placement, persistent-support rejection, door-foot and Python region tests.
Own-profile016 production EndFrame passed north512pixel checks, south attachments,
pot support, schema30/cache/plane/camera assertions. Arm64 Release and Debug builds
succeeded into `build/release/` and `build/debug/`; `git diff --check` passed.
Fresh gameplay pickup/carry/throw/impact and south-view comparison remain pending;
synthetic lifecycle regressions do not replace that evidence or complete-room v2.

Parent requested diagnosis of introduced1288→1303conflicts and529→551missing
supports before builds. They were **real rule-scope defects**, not merely
preexisting failures to accept. Ablation with identical immutable016 metadata
isolated ALL increases to potential-pot support; turning that capability off
restored the baseline exactly. No current authoring profile substitution.

Three failing stages were identified and repaired:

1. Feeding post-removal scratch attributes into the ordinary Link movement solve
   allowed pot-cleared bodies to change component connectivity. It also reused
   the scratch table for wall/cap/border ownership, beyond its evidence scope.
2. New potential bodies were counted as current reachable support; already-bad
   room0c0 then acquired22additional required/missing cells. This was a changed
   denominator, NOT22new missing ordinary reachable supports, but the underlying
   solve scope was still wrong and is removed.
3. Promoted pot Obstacle nodes were added to the structural graph, so0c0's
   already-conflicting graph acquired11additional bad nodes despite identical
   signed contradictions. Potential supports must be terminal consumers, not
   geometry propagation or relative-datum nodes. New support also triggered
   ambiguous identity-less border fallback in0c2/0cb/0cc/0dc.

Final rule retains original collision for ALL ordinary movement/structure/fallback
passes. Separately enumerates newly legal complete16x16 Link bodies touching a
proved removable footprint. Each body must touch exactlyONE original movement
component; twoowners/contradictory claims reject (no component merge, no potential
body flood). Required blocked-footprint cells with no observed body are separately
unobserved. Solved potential placements consume only that original component's
base AFTER its full solve. Pot nodes do not enter the structural graph; unknown
pot support cannot be rescued by nearby border fallback. Counts and every
potential placement/owner/status are now explicit in runtimeJSON and v2; missing
persistent support also fails the complete gate, but is not relabeled reachable.

Final346contexts/319rooms corpus, sameprofile before/after:

|metric|before|final|
|---|---:|---:|
|structural conflicts|1288|1288|
|missing movement supports|529,0|529,0|
|solved/unknown BG2 walls|88000/256552|88000/256552|
|contexts increasing conflict or movement missing|—|**0**|

149changed contexts,3011changed layer/plane bases. New separate potential-support
denominator4380plane0 usages:3010solved,1370missing =596unobserved bodies,
752ambiguous body owners,22original-component conflicts. This is NOT hidden
denominator reduction: the unresolved potential support remains explicit and
is a complete-room blocker. It supersedes initial9new016support cells: conservative
final016 has6potential cells (four pot cells plus two body-eroded cells),all6solved
at24,plane1 remains0; ordinary solve denominator stays[550,3506]. Three prior
extensions atx42 no longer have unique direct body ownership and are not guessed.

Independent ALTPRV1+ROM atlas audit of all346contexts attests875/875 exact indexed
pot footprints to `RoomDraw_SinglePot` handlers;0unproved/overwritten claims,
0unrelated liftable templates. Audit includes canonical light/dark variants and
transformed identity. Actual ROM repeated fake-pot handlers also invoke SinglePot
and are liftable only when completed indexed attributes corroborate them;
same artwork with ordinary collision is rejected. Dynamic states remain outside
this default-context corpus, not silently assumed covered.

Exact five-context constraints saved as`room-0c{0,2,b,c}-...-{before,after,no-pots}.json`
and0dc counterpart.0c0 final required[1389,0],missing[529,0],badnodes750,
equalitymismatches116 ALL match baseline; all64recorded signed constraint issues
are byte-for-byte equal, residuals±40. Potential22cells reference that already-bad
component and remainmissing; no constraint introduced/suppressed.0c2conflicts34,
0cb46,0cc56,0dc42 all restored EXACTLY; originalrequired/missing/componentissues
and signedconstraintrecords equal baseline. Initial1/6/2/6additional conflict
counts are removed, not described as preserved. Machine followup asserts these
equalities, not just total visual coverage. Other preexisting walls/conflicts
remain unresolved; baseline equality is no evidence that their geometry is correct.

### All eight south doorway regions repaired

Same exact source aliases29f0/2a20/2a80/2af8,writer107,complete12word footprint,
origin14/59. Existing immediate flanks are preserved obstacle-marked curved
wall/trim assets, so they cannot prove a wall seam. No reclassification/array
replacement or copied northbase was used.

The actual innermost jamb part14/59 and17/59 faces NORTH toward its landing:
Vflip transforms canonical finalrow0 into displayed north0. Six marked seam
pixels are0 on the jamb and0 on adjacent landing14/58 and17/58, whose ordinary
Link-body component is independently solved24 onplane0. Full neighbor relief
0..1 is preserved; only those exact seam samples participate. Handler part
coordinate, inwardaxis, actualoccupied component, ordinaryattribute00,≥2marked
samples and exactzero local samples are all mandatory. Wrong part, specialblocker,
unowned/unknown/nonzeroseam rejects. This is a direct proved FOOT contact, not
nearest-floor search or arbitrary neighbor-copy. It supplies framebase24; existing
same-assembly authored seams then solve both middlejambs and allfour outerband
placements. Independent plane1 base0 stayshidden; visible owner is24.

Complete016 frames now**32/32,0missing,0frameissues**, bothnorth/south userIDs.
South0..7band→24..31 onframepixels;16..23outerband→40..47; excluded pixels remain
authored0. ProductionEndFrame exact016 asserts all8south attachments/base/plane
and preserves priornorth512actualpixel checks, schema30/palette/cache/plane/camera.
Fresh SOUTH gameplay is not in016viewport; application proof is complete placement
plus production path, not a fabricated matching south capture. Allarrays unchanged.

Final fullv2 same immutable inputs +NEWproductionALTPHM3 **exit1**:
1248/1248reachable,planes[550,698],76overlap,12/12fallback,673/7walls,
1structuralconflictrecord(runtime3),2compiledmissing/67mismatch,32/32frames,
separatepersistent6required/0missing. Remainingroomfailure NOT waived.
Fixtures055/060 completev2 retain439/439 and711/711,exit1;061exit2 independent
diagonal-preview proof gap. Live138packets:32own-profile replays,99missingcomplete
evidence,7stale/unsupported; no ALTPDA1 gameplay lifecycle evidence yet.

New focused tests `dungeon_persistent_support_rejection_test.cpp` and
`dungeon_door_foot_test.cpp` pass optimized+ASan/UBSan: incompatibledualowners,
no component merge/constraint/fallbackcorruption, fouraxes/HV footcontact,
wrongpart/nonzeroseam/specialblockerreject. Lifecycle denominator regression now
asserts ordinarycount equality and separatepotentialcount; pot test additionally
rejects mixed70indices. Existingdoorframe/multiBGtests pass; Pythonnewregion1,
doorframeaudit1,validator23pass. Finalcorpus before/after constraint assertions pass.

Additional changedfiles this followup:sharedframe/remaster,complete runtimeadapter,
validator,audit_high_corner runner(extra counters),gate/probe/production regression,
lifecycle/pot tests; new `alttp_door_pot_followup.py`,
`alttp_audit_pot_producers.py`,twofocusedC++tests; thisreport. Artifacts use sameOUT;
`offline-corpus/corpus.json` now containsFINALtotals/reasons/zero introducedcontexts;
`pot-producer-corpus.json`,fiveexactconstraintsets,completegate/production regenerated.
Parentbuild/interactive comparison stillpending. No staging/commit/reset/excludes,
protectedmetadata/schema30/artwork/frontend edits. Runtimechanges final for parent
verification, but room remainsincomplete, never productionapproved.

Implemented reusable **effective** door-region and resting/carried/thrown pot
handling. No authored profile/semantic arrays were edited. Complete validation
v2 still exits **1**; this is NOT room approval or permission to publish a map.
Parent owns arm64 Debug build and independent tests. No build, staging, commit,
reset, excludes or frontend/schema/palette edits by this task.

## Pinned latest inputs

Latest available packet is017, without complete sidecars. Latest complete062 is
016, schema30, SCN1 absent. All analysis uses016's OWN serialized profile, not
the concurrently modified authoring profile. USA ROM SHA256:
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Room98, previous97, settled submodule0/collision0; Link1232,3155,plane0;
scroll1112,3073, both graphics bases16384, map bases[4096,0],sizes[3,3],rise50.
Entry cell−1/predecessor65535: previous97 is NOT a historical height anchor.

|input|SHA256|
|---|---|
|016 packet|4c2cdab652ccb61d0baebd67f18e029abe212216a81359ae1a2cdfd5c30503bd|
|profile|85e45e8b19b74b72c49803887847b4841f71efd506c4a0e11f8f166fc0604e7b|
|state|7b7de7e71bb1630818a31c1b921402a7096bc3c9099e48e11f0ee50d3034ecd5|
|complete input|919bf488bfbf9b0887dc08ee5e8c0e5253d3e945b9cfb4a23e8eb07e8ea3a24b|
|templates|be6a8a9c5b436772f96700973307ec81fc31843e06118e0e567b658e59bc5ddd|

Own-profile FNV `dbe975eeca6fbe31`.87489 main/sub BG pixel identities, flips,
canonical pixels and camera samples match complete inputs.8192 WRAM/PPU words
agree. Independently matched reference062 has8192 identical final words and8192
identical collision bytes. Its ALTPRV1 attests current dispatch, not history.
Do NOT use `build/remaster-geometry/live016-review/` as062 evidence: inspected
state there is room96/060 despite directory name.

## Door failure class and runtime repair

`3082443c6bd2ebc1` and `b225fad8c5498046` have preserved16..23 local arrays,
but no activated Door role. `482a1674f4994ba7` has Door role and local0..7,
but no frame mask; its last two canonical columns have zero height/occlusion.
Existing full-door recognition required activated marked metadata, so these
owned band placements could not participate in the frame seam solver.

North BG2 origin30/4: final writer106, phase3 ordinal9, raw16, source offset281.
All12 final words match ordinary north templates2716/2746/284e (identical source
aliases, not distinct owners). South origin14/59: writer107,phase3 ordinal10,
raw97,source283. Arrays are assembly-local bands, NOT necessarily0..7 per tile.

Shared rule accepts only complete ROM footprints, actual axis threshold evidence,
frame-part coordinates (outer band or outer side row/column), canonical exact
unit ramps, compatible authoring role, and no explicit mask. Existing masks
including explicit empty remain authoritative. Transparent/excluded pixels may
be omitted only with exact0occlusion AND0height; ambiguous/partial occlusion,
mixed opaque profiles and opening/tread part coordinates reject. Priority-only
writes survive; identity/flip overwrite rejects. No room/hash elevation list.

The existing all-sample signed seam and visible-owner solve is unchanged.
Application now consumes the placement attachment mask even when the asset has
no Door role; it checks producer hash/flips and keeps opening pixels unchanged.
Door regions cannot accidentally receive mixed-floor local overrides. Occlusion
is included in geometry cache identity because it now constrains exact exclusion.

016 north frame: all eight required region placements attach plane0 base24.
Outer local16..23 composes to40..47; lowest side local0..7 composes to24..31 on
the48marked pixels, excluded16pixels remain0. Production EndFrame verifies512
actual north-door main/sub canonical pixel samples, no placement-local rewrite,
schema30 roundtrip, unchanged stock colors/palette, cache/plane/camera lifecycle.
South frame still lacks consistent solved owned seams: eight region placements
remain unknown in the COMPLETE denominator, not default solved at0.

**Authoring guidance:** no reauthoring of door height arrays is needed merely
because they differ from wall layout. Preserve their assembly-local bands and
let compatible seams solve the contextual base. Explicit Door role + frame mask
remains the preferred canonical annotation for mixed frame/opening art; absent
mask is not generally a full mask. The narrow ROM-owned exact-band rule restores
this legacy case without globally changing assets or flattening authored stairs.

## Actual pot game logic and support

Reference engine `src/dungeon.c`:

* `LoadType1ObjectSubtype3` case2f (atlas subtype1/object47), lines1880–1882.
  Room062 writer70,phase1 ordinal56,raw44963,source170,object40/43.
* `RoomDraw_SinglePot` `$81:b395`,3611–3623 installs replacement state1111,
  four replacement floor words0d0e/0d1e/4d0e/4d1e, and draws2x2 templatee82
  (dark variante92). Final words0d28/0d38/4d28/4d38 exactly match e82.
* Completed attribute override4016–4022 writes70+object index over allfour cells.
  Attribute70 alone is NOT a pot classifier (other liftables share the table).
* `Dungeon_LiftAndReplaceLiftable` `$81:d9ec`,5532–5557 selects indexed state,
  reveals contents and calls `RoomDraw_16x16Single` `$81:d828`,5494–5502.
  That routine installs recorded floor words and their completed attribute.
* Hammer/sword break5583–5601 also restores floor before spawning break effects.

Shared classifier requires the intact transformed2x2 ROM template, allfour
identical indexed70..7f attributes on a specific plane, authored local relief,
and no incompatible semantics. Fake/decorative art with ordinary collision is
rejected. Resting pot becomes placed Obstacle without hash-wide obstacle labels.

Structural solver enumerates persistent post-removal support separately from
current movement reachability. ONLY proved removable footprint cells are changed
to ordinary support in invocation-local scratch; input collision and complete
doorway movement graph remain blocked and unchanged. No neighbor-copy, nearest
floor or artwork flood. The same Link-body support solve and signed structural
constraints determine the floor below. Local pot relief remains untouched.

016: pot40..41/43..44 base0→24; allfour plane0 support bases24, independent
plane1 bases0. Five additional formerly eroded support cells beside/behind it
resolve24, for nine new persistent plane0 support cells (550→559). No BG1 base
changes. Pot local0..5 and0..4 compose24..29/24..28. This does NOT establish an
arbitrary user-reported viewport floor should be lowered: the exact owned static
state here places the pot on the proved24 support. Compare pickup in matching
gameplay before making a different claim. Post-pickup local replacement graphics
currently lack profile arrays; support is proved, their effective local source
remains a metadata gap rather than generated zero metadata.

## Carried/thrown lifecycle and producer identity

`src/player.c`: `Link_PerformThrow` `$87:b11c`,3486–3527 removes terrain and
maps returned lift attribute50 through `kLink_Lift_tab` to index2.
`src/sprite.c`: `Sprite_SpawnThrowableTerrain_silently` `$86:8156`,1026–1062
spawns typeEC,state10,C2,floor=Link plane. **C1 is bush, not pot**.
`SpriteModule_Carried` `$86:de83`,1570–1617 sets X from Link/held-frame table,
sets `sprite_z` from held-Z table, sets Y=`LinkY+8-(LinkZ+1+animationLift)`, and
updates floor=Link plane. `CarriedSprite_CheckForThrow` `$86:df6d`,1619–1648
sets state=`sprite_unk4` (9), clears carry frame and gives signed XY/Z velocities.
`Sprite_PrepOamCoordOrDoubleRet` `$86:e41e`,1835–1857 subtracts sprite_z from
Y only for rendering. `sprite_main.c` `Sprite_EC_ThrownItem` `$86:aae0`,9602–9627
draws one16x16 OBJ; C2 selects name46,palette6. Children46/47/56/57 share ONE
object base. `Sprite_CheckTileProperty(k,68)` uses index52 samples X+8,Y+12.
Scatter/break9649–9679 clears state0 and creates garnish debris; garnish is NOT
the same owned pot actor and is explicitly outside this lifecycle implementation.

Live verified-USA adapter fields:

|field|WRAM|
|---|---|
|sprite Xlo/Xhi|0d10/0d30 +slot|
|sprite Ylo/Yhi|0d00/0d20 +slot|
|sprite state/type/C|0dd0/0e20/0db0 +slot|
|sprite floor/Z|0f20/0f70 +slot|
|Link XY/Z/plane|22/20/24 (words),ee|
|BG2 actor scroll copies|e2/e8 (words)|
|producer OAM/extended size|0800+4slot /0a20+slot|

Runtime matches unique active EC/C2 producer position minus actual gameZ to its
complete OAM name46/palette6/16x16 slot, then requires matching FINAL PPU OAM
position/name/flips/palette/size. Instance child number/palette/flip must agree;
hash identity alone never selects an actor. Duplicate owners reject. Per-frame
evidence is value-owned and not cached geometry; position/animation do not
trigger complete room rebuild. No cross-frame sprite-slot ownership survives.

Carried: independently proved grounded Link support at X+8,Y+16, active plane,
plus spriteZ + exactly recovered `(LinkY+8-spriteY)` game lift. Link z/animation
lift is NOT added again. Airborne/nonordinary Link lacks proved owner support
and is rejected. Thrown/grounded sprite: own floor and X+8,Y+12 support, plus
spriteZ once. Unknown support/ambiguous ownership cannot use a screen-bottom
fallback within the accepted producer path. Existing authored OBJ local arrays
are preserved. Other unrelated objects retain the preexisting generic path;
this task does NOT claim that legacy screen-bottom/Link-plane path is correct
for all sprites or that Link already has a complete airborne-height adapter.

Additive910-byte `.terrain-input.bin`, magicALTPDA1, pins actor/OAM fields at
the same first-frame evidence boundary. Existing schema30 and ALTPDI1 unchanged;
old captures remain readable and explicitly lack lifecycle evidence. Pin/copy/
readback regression passes. Paused frontend capture still bypasses complete
evidence writing; no new sidecars were fabricated for016/017. Frame-final actor
updates may fail exact OAM-position matching: fail closed until fresh trace shows
parity, rather than tolerating distance or tile identity. Fresh resting→pickup→
carry→throw→impact captures with producer sidecars remain REQUIRED to establish
real game execution and both authored sprite tile halves. Break garnish remains
an explicit unsupported effect denominator; owner state reset is covered.

**Pot authoring guidance:** obstacle describes intrinsic relief, not immobility.
Do not bake24/50 into either array and do not relabel it Floor to fix blocked
reachability. Leave current arrays as authored. The actual liftable producer
proves placed obstacle/support; dynamic lifecycle comes from game state. A future
canonical obstacle annotation is appropriate only for unambiguous reuse and does
not replace producer ownership. No such annotation was silently applied here.

## Gates, corpus and regressions

Full016 v2 uses matching ALTPPD1/ALTPAT1, OWN profile, complete graph seeded from
actual captured Link, both directed reference stair edges against identical
maps/attributes, runtime JSON and NEW production signedALTPHM3. **exit1**:
1248/1248 reachable supports,planes[550,698],76overlaps,0missing supports,
12/12fallback,673solved/7unknown walls,1conflict record (runtimecount3),
2compiled missing/67mismatch. Frame denominator32,solved24,missing8south,issues0.
Directional seams/ownership remain explicit; no gate waived.128+128 actor/OBJ
producer identity/lifecycle is not proved by the reachable-support denominator.

Offline corpus346complete contexts/319rooms, same immutable016 research profile
on both solvers:167changed contexts,13221 changed layer/plane bases; solved walls
88000→92957,unknown256552→251595,structural conflicts1288→1303,support missing
[529,0]→[551,0]. Increased missing/conflicts are unresolved exposed ownership/
constraint failures, NOT silently accepted room coverage. Corpus current BG2
doorframe-region attachments are not included in those tile-wide base totals.
Room017 unsettled; dynamic/nondefault states, full movement outside fixtures and
matching Snes9x remain unsupported. These totals are not complete geometry.
Live138packets:32own-profile complete replays,99missing evidence,7stale/unsupported
rejected. No current profile substitution. None supplies new ALTPDA1 lifecycle
evidence. Fixtures055/060 with explicit016 metadata: v2exit1,439/439 and711/711
support,walls1602/12 and1017/9;061exit2 independent diagonal-preview proof gap
remains. Existing060028/032/034 exact regression assertions all pass.

PASS tests: new door-band all directions/HV/transparent exclusion/explicitempty/
opening/overwrite/authoredmismatch, pot allflips/planes/index/overwrite/fakepot,
lifecycle resting→removed support invariance/carried/thrown/landing/airborne
owner rejection/duplicate/reset/unrelated same artwork, value-owned sidecar
roundtrip; allthree ASan/UBSan. Existing doorframe runtime incl split BG ownership,
stair role,32highcorner,48obstaclecomponent,semanticcache PASS. Python new region
audit1,doorframeaudit1,validator23 PASS. Production016 application/cache/plane/
camera and schema30/palette PASS. `git diff --check` PASS. No GUI review/build.

## Files and artifacts

This task modifies `gfx.cpp`, shared `remaster/frame.h`/`remaster/remaster.h`,
`remaster/dungeon_placement_equivalence_test.cpp` (new field inventory),
`tools/ALttP/alttp_live_height_repair_probe.cpp` (optional lifecycle evidence),
`alttp_room062_endframe_probe.cpp` (exact016 effective regression),
`alttp_validate_room_heights.py` (independent full region denominator/proof).
Adds three `remaster/dungeon_{door_band,pot_placement,terrain_lifecycle}_test.cpp`,
`tools/ALttP/alttp_door_pot_{evidence,gate,results,summary}.py`,
`alttp_door_pot_probe.cpp`, `test_door_pot_regions.py`, this report. Concurrent
schema30/artwork/frontend/profile/semantic/prior060061062 changes preserved.

`OUT=/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/door-pot-lifecycle`
contains own-profile before016/after016JSON+.s9xrmf+heightPPM; evidence016 pins/
writers; production016JSON/.s9xrmf/main-subPPM/signedmap; fullraw/attr/profile/
state/graph/runtime,validation-v2JSON/SVG,gateJSON+summary; offline/live corpus;
fixtures,exact060preservation packets; syntheticterrain sidecar (NOT gameplay).

Principal reproducible commands (repo root; OUT as above):
```sh
c++ -std=c++17 -O2 remaster/dungeon_terrain_lifecycle_test.cpp -o "$OUT/lifecycle-test"
ALTTP_TERRAIN_TEST_EVIDENCE="$OUT/synthetic-terrain" "$OUT/lifecycle-test"
c++ -std=c++17 -O2 remaster/dungeon_door_band_test.cpp -o "$OUT/door-band-test"
"$OUT/door-band-test"
c++ -std=c++17 -O2 remaster/dungeon_pot_placement_test.cpp -o "$OUT/pot-placement-test"
"$OUT/pot-placement-test"
python3 -B -m unittest discover -s tools/ALttP -p 'test_door_pot_regions.py'
c++ -std=c++17 -O2 tools/ALttP/alttp_room062_endframe_probe.cpp -o "$OUT/endframe"
"$OUT/endframe" "$HOME/Library/Application Support/Snes9x/Remaster/frame_room062_016.s9xrmf" "$OUT/production016"
```
Full exact validator command is machine-recorded in`$OUT/gate.json`; corpus and
fixture commands use documented existing runners with immutable016 profile.

Smallest next action: parent build/independent tests; investigate failing south
frame seams/compiled-source alignment and broader pot-connected constraint
conflicts. Then collect producer-pinned lifecycle evidence through running
capture (paused evidence gap remains). Room MUST NOT be handed back as successful
or publication/capture-approved until complete v2 exits0 and gameplay parity is
reviewed across both planes/cameras. No claim that synthetic lifecycle tests are
matching gameplay or that unknowns are a coverage goal.
