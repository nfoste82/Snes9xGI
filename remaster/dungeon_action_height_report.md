# Room061/048 and room062/019 — reviewable but incomplete

Both reusable runtime repairs implemented. No room approval: mandatory v2 exits
1 for061 after the independent partial-diagonal proof follow-up,1 for062.
Parent independently passed semantic diagonal, terrain lifecycle, three partial
proof tests and23validator tests, plus own048/019 production EndFrame probes
(112531/101460 matched BG samples; allfour carried children/124visible pixels).
Arm64 Release and Debug builds and `git diff --check` passed. Current user profile
now contains allfour carried-child intrinsic height arrays; historical019 still
retains its own missing-source evidence. See the independent validator report.
Parent owns builds and
independent tests; no renderer/frontend/profile/schema30 edits by this worker.
Concurrent `remaster/profiles/ALttP/alttp-profile.toml` changes appeared during this task;
they were not touched, consumed as replacement evidence, or reverted.

## Exact evidence / root causes

Both captures use OWN profile serialized FNV `90f968a81fe8e52f`, SHA256
`769f20ff708168e900a093d05cf7deeaa8176dead1ae7372734358d765ced137`, rise50,
USA ROM `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`,
both graphics bases16384, map bases[4096,0],sizes[3,3], settled submodule0,
collision0,plane0. Complete8192 WRAM/PPU words agree. Full per-input SHA256,
state, exact validator commands and exits: `$OUT/live{048,019}/gate.json`.

*061/048*: room97,previous98,Link606/3346,scroll512/3239,cache key
10041970577072404872,epoch913.112531 main/sub BG identity/flip/canonical camera
samples match. No entry attestation. Seven-strip columns8..11 already correct;
the missed southwest columns2..7 have cardinal-overwritten feet. Their surviving
semantic SE strips cannot establish their OWN foot, so extent rule rejected them
and left internal cap profiles17..24/9..23/1..15,base255. Cold/full/cache outputs
agree: not an eight-state restoration regression. Shared tangent phase attachment
now requires matching canonical same-band identity, identical H/V semantic
ownership, exact clipped top and independently foot/top-proved companion. Does
not cross/reclassify cardinal overwrites.42 placements restored across both mirrored
partial assemblies; BG2 solved walls914→956,unknown2 unchanged,conflicts0 unchanged.
Internal sources become41..48/33..47/25..39,base0. Authored arrays untouched.
`local048{,-after}.json`, captured/recomputed PNG/PPM and production packets show
real effective changes, not diagnostic tint. Field cache unchanged and incremental
versus forced extraction equality checked; parent renderer investigation unnecessary
for this reproduced geometry defect.

*062/019*: room98,previous97,Link1305/3408,scroll1185/3301,actor scroll1185/3300,
LinkZ=-1,grounded attested,cache key12506207846107457970,epoch1172.101460BG samples
match. Actual producer15:EC/C2,state10,XY1305/3413,Z16,plane0;unique final OAM12:
XY120/97,name46,palette6,size16x16,flags0x6c. Matcher wrongly required no H/V flips,
but game producer preserves facing flags. Now instance flips must equal FINAL
OAM producer flips (not be globally false); disagreement/duplicate owner reject.
All four children46/47/56/57,124visiblepixels,base0→43=solved support24+spriteZ16+
recovered animation lift3. No doubleLinkZ. Game `SpriteModule_Carried`86de83 sets
Y=LinkY+8-(LinkZ+1+frameLift),Z=held-frame table; `Sprite_EC_ThrownItem`86aae0
preserves upper OAM flag bits. Producer/OAM timing matches EXACTLY in019.

The four OBJ child hashes lack authored height sources:
`21c1767459218910`,`a9b04900f78069e6`,`ac36befaa085a824`,`a98b371291ed848a`.
No BG-pot relief was copied onto them. Base is repaired, intrinsic geometry still
missing; rendering a flat base43 is not a complete pot shape. Visible authored
Link maximum42 (84pixels,slots58/66/67), versus game-derived carried base43.
This one-frame agreement does NOT prove a highest-Link-point rule: visibility,
body/shield ownership and animation transform lack an assembled body attestation.
Keep producer game lift rather than arbitrary visible maximum. Pedestal/replacement
BG remains independently placed; not counted as a second EC owner. Remaining
capability: canonical reusable OBJ-child local profiles plus verified assembled
Link body/support identity if replacing the game transform with body geometry.

## Tests / corpus / gates

* Production EndFrame own048 exact southwest pixel ramps incl partial columns;
  own019 exact actor/OAM pins/four children/base43/missing sources; second-frame
  cache hit and forced-full packets/maps match. Stock colors and own profile remain.
* Semantic extent40cases extents5/7/9/12/20,allflips,partial foot overwrite,
  unrelated identities/unknown top/transformed ownership rejection. Lifecycle
  allOAMflips,animation,unknownsupport,overwritten transform,multipleactors/slots,
  airborne/reset/throw negatives. ASan/UBSan passed; existing variable032 and
  production geometry cache tests passed; fieldcache dirty-row test passed.
*96available060/061 packets:32complete,29exact-equivalent,3rejected stale own
  evidence,64missing complete sidecars. Own062019 passes separately (only two
  actual ALTPDA1 captures available:061048/062019).062016 own replay also passes.
*346contexts/319roomIDs,default states,bothBGs/planes,synthetic animation differential:
 339equivalent reuses/7fullfallbacks,all inventories/packets equal full. Residual
 unknownwalls256552,conflicts1288,missingmovement[529,0] unchanged. No context
 count regression. This corpus has no live carried actors, and is not every
 switch/water/door/animation state or the missing320throom. Actual latest captures
 carry semantic partial-assembly evidence unavailable in many corpus placements.
* Mandatory fixtures own048 profile:055exit1,439/439supports,1602/12walls;
 060exit1,711/711supports,1017/9walls,1conflict;061exit2 independent diagonal
 proof gap. Exactlive048 complete graph collision pin matches; v2exit2 same gap.
 Exactlive019 graph recomputed after four indexed-liftable→ordinary collision
 changes; directed prior reference stair edges retained (fresh parity unproved).
 v2exit1:1253/1257validsupports,planes[559,698],overlap76,walls673/7,
 4missinglocal-sourceplacements,1conflictrecord,compiled8missing/67mismatches.
 Compiledmaps solver-derived,not claimed production-map approval. Exact commands
 stored in gates.json. Reachable validator does not prove OBJ child geometry.
* Speed cache unchanged.40measured+10warmup own048-profile recorded fiveVRAM-state
 cycles:before median8.089/p958.498ms;after8.530/p958.960ms (after overlapped fixture
 run;not controlled performance regression proof).19exactstatehits,20publications,
 no equivalence shortcut, retained8statecache. No liveFPS/build claim.

## Commands / artifacts

Repository root, preexisting directory:
```sh
OUT=/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/action-height-review
CAP="$HOME/Library/Application Support/Snes9x/Remaster"
ROOT=/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode
c++ -std=c++17 -O2 remaster/tests/dungeon_geometry_cache_capture_probe.cpp -o "$ROOT/action-capture"
c++ -std=c++17 -O2 -DALTTP_TEST_DISABLE_GEOMETRY_REUSE remaster/tests/dungeon_geometry_cache_capture_probe.cpp -o "$ROOT/action-full"
"$ROOT/action-capture" "$CAP/frame_room061_048.s9xrmf" "$ROOT/action048-after"
"$ROOT/action-capture" "$CAP/frame_room062_019.s9xrmf" "$ROOT/action019-after"
python3 -B remaster/tests/dungeon_geometry_cache_captures.py --captures "$CAP" --enabled "$ROOT/action-capture" --full "$ROOT/action-full" --output "$OUT"
c++ -std=c++17 -O2 remaster/tests/dungeon_geometry_cache_probe.cpp -o "$ROOT/action-corpus"
python3 -B remaster/tests/dungeon_geometry_cache_corpus.py --corpus "$ROOT/diagonal-sets-priority" --profile "$CAP/frame_room061_048.s9xrmf.profile.toml" --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc --probe "$ROOT/action-corpus" --output "$OUT/corpus-after.json"
"$ROOT/diagonal-venv/bin/python" -B remaster/tools/ALttP/alttp_action_review.py --captures "$CAP" --output "$OUT" --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc --graph061 build/remaster-geometry/surface-audit-room-local/room-061-doorway-graph.json --graph062 "$ROOT/door-pot-lifecycle/live-graph.json"
git diff --check
```
Replay inputs produced by `alttp_replay_live_height_evidence.py` with OWN capture,
`--solver "$ROOT/action-solver" --output "$OUT/live048"` (or019) and same `--rom`;
solver built from remaster/tools/ALttP/alttp_runtime_room_heights.cpp. Narrowtests compiled
`-std=c++17 -O2`,sanitizers`-O1 -g -fsanitize=address,undefined`.

Smallest next actions: parent builds/independenttests; independent validator
proof for partial semantic diagonal attachment; canonical OBJ child metadata;
then matching pickup/throw/impact and multiple-camera gameplay comparison. Do
not call either room successful or publishable while complete gates fail.
