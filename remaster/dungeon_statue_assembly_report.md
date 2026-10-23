# Room0x050 — reviewable statue repair; complete room still blocked

## Parent verification and final builds

Independent optimized assembly regression passed, including flips, separate
support50/0 copies, runtime composition and rejected incomplete/ambiguous feet.
Fresh own-capture probe independently reproduced all12 statue parts at base0,
unchanged local relief and256 actual visible head usages. BG2 coverage remains
419solved/489unknown; this is not complete-room approval.
Arm64 Release/Debug builds and diff check passed after the statue repair.
Build log: `/Users/nic.foster/.local/share/opencode/tool-output/tool_1292d645a001bNc24WBn9N5ZGK`.
Independent artifacts: temporary root `parent050-statue-result.*` and
`parent050-obstacle-test`. Running user process was left untouched; fresh live
comparison requires reopening `build/release/Snes9x.app`.

## Exact evidence and corrected diagnosis

`v1:4bpp:8044813840be1f26` is BG1, not the unresolved BG2 wall graph.
Own050 contains two intact ROM0xe26 **2x3** statues at(44,11)/(54,11).
Each has2headcells,2middlecells,2feet. Middle hashes94f7419943e208a7/
19b42a376beb664f; feet72661475776fbe7a/2efa8ecc16538a5b.
Ownprofile head0..23,middle0..15,feet0..5/7; transparentzero pixels preserved.
All12finalwords match reference provenance: writer58phase2ordinal6offset112
raw11440 and writer59phase2ordinal7offset115raw11480. VerifiedROMhandler38 and
subtype2:1d/21/26 dispatch RoomDraw_1x3_rightwards(2), width2,height3,
column-major template. Historical live dispatch stream not present/claimed;
current exact ROM footprint corroborates matching reference writers.
Ownsixsidecars/ROM/profilepins remain as in dungeon_room050_report.md/gate.json.

## Shared runtime repair

Extended existing exact freestanding footprint recognition, NOT hash grouping:
unique whole footprint assemblyID/template/height/flip in Placement, included in
sameGeometry,inputkey,equivalenceinventory. Exact2x3statues/2x4columns only;
wallcolumns/decorations excluded. ROM-ownedstatues override staleWall/WallCorner
placementclassification without assetrole/array edits. Completeauthoredheight on
allmembers required; explicitfloor/stair/door rejects. Priority-only writes allowed;
tile/flips overwrite or ambiguoustemplate ownership rejects wholeassembly.

Parts are TERMINAL support consumers: no structuralnodes/edges, localrelief cannot
constrain floors or borrow highwall seams. Transformedcanonicalfootrow (Vflip
movesfootnorth) requires BOTH width2groundcontacts from reproducedbody support.
Onecomponent/plane/signedbase owns allcontacts. Differentcomponents/planes/bases
conflict; missingfoot unknown. Base applies ONCEtoallparts, no ramps/localrebasing/
headproximity. Actor support under nonblocking artwork remains independent;
support plane arrays are not replaced by statuegeometry. Pot consumers unchanged.

Actual12/12parts now share footbase0; allcanonicalarrays unchanged.256actual
headpixel usages in producedschema30packet verify offset0/nooverride. Read-only
priorpart-policy ablation (same improved classification) gives headbase8,
incorrectly adding8 atop authored16..23face. Syntheticindependentcopies show
highstatuebase50/lowstatuebase0 on everypart, includingheadbeyondfloorcontact.
This is real runtimecomposition repair, not inspector-only grouping.

OwnBG2walls remain419solved/489unknown and0conflicts. Those are genuine wall
blockers, NOT these BG1statues. Prior broad wall explanation did not diagnose this.
Historical exactclassification already recognized these parts as obstacles;
their missing feature was wholeassemblyterminalsupport, not absent assetarrays.

## Corpus and gates

346contexts/319IDs,bothBGs/planes/defaultstates:80recognizedstatueassemblies,
480placedparts,360solved/120unresolved,0mixedpartbases.339reuse/7fullfallback,
exactallmap/diagnostic/packet equivalence. Comparedshared-shell-onlybaseline:
BG2unknown256158→256182,conflicts1288→1289. Concreteaffectedcontexts036:
700→718unknownwalls;0a8:750→756;072conflict0→1. Removingstatues as graphanchors
reveals previouslyborrowedwallcontacts; wholefootambiguity nowexplicit. Illegal
statueconstraints are not restored for attractive counts. MissingroomID/dynamic
switch/water/doorstates and120unresolvedparts remain unsupporteddenominators.

Owncomplete-roomv2 rerun exit1:376/376reachable supports,4/4fallback,419walls/
489unknown,0structuralconflicts,existingcompiled4missing/4mismatches. ThisBG2gate
does NOTproveBG1statuecoverage; ownproductionprobe covers12BG1parts+256headusages.
Fixtures060exit1,061050/051exit2sameindependentdiagonalproofgap. No room approval
or newcapture-approvalhandoff. No freshGUI/gameplaycomparison.

## Tests and exact commands

Newregression O2andASan/UBSan PASS:allH/Vflips,independent50/0copies,
wall-mislabelcorrection,exactruntime50+23composition,localpreservation,
partialoverwrite,missingwholefoot,ambiguousduplicatedtemplate,samebaseambiguous
twoplanes,conflicting50/0planes,cachefieldinvalidation. Existingpot,terraincarried/
resting/thrown/landed,obstaclecomponent,sharedshell,cache,entry,transition PASS.
DiffcheckPASS. Parentownsindependentbuild/verification; their previousbuild
predates thisstatuechange. Noappstop/launch,staging/commit/cleanup/profilechanges.

Repo root; ROOT=/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode,
CAP="$HOME/Library/Application Support/Snes9x/Remaster",OUT="$ROOT/room050-review".
```sh
c++ -std=c++17 -O2 remaster/tests/dungeon_obstacle_assembly_test.cpp -o "$ROOT/room050-statue-test"
"$ROOT/room050-statue-test"
c++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer remaster/tests/dungeon_obstacle_assembly_test.cpp -o "$ROOT/room050-statue-san"
"$ROOT/room050-statue-san"
c++ -std=c++17 -O2 remaster/tests/dungeon_room050_probe.cpp -o "$ROOT/room050-classify"
"$ROOT/room050-classify" "$CAP/frame_room050.s9xrmf" "$OUT/statues-effective-final"
c++ -std=c++17 -O2 -DALTTP_TEST_PRIOR_OBSTACLE_PART_SUPPORT remaster/tests/dungeon_room050_probe.cpp -o "$ROOT/room050-statue-before"
"$ROOT/room050-statue-before" "$CAP/frame_room050.s9xrmf" "$OUT/statue-prior-part-policy"
c++ -std=c++17 -O2 remaster/tests/dungeon_geometry_cache_probe.cpp -o "$ROOT/room050-statue-corpus"
python3 -B remaster/tests/dungeon_geometry_cache_corpus.py --corpus "$ROOT/diagonal-sets-priority" --profile "$CAP/frame_room050.s9xrmf.profile.toml" --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc --probe "$ROOT/room050-statue-corpus" --output "$OUT/corpus-statue-after.json"
"$ROOT/diagonal-venv/bin/python" -B remaster/tools/ALttP/alttp_statue_evidence.py --review "$OUT" --profile "$CAP/frame_room050.s9xrmf.profile.toml" --reference "$ROOT/diagonal-sets-priority/room-050-entrance--1.raw"
c++ -std=c++17 -O2 remaster/tools/ALttP/alttp_runtime_room_heights.cpp -o "$ROOT/room050-solver"
"$ROOT/diagonal-venv/bin/python" -B remaster/tools/ALttP/alttp_replay_live_height_evidence.py "$CAP/frame_room050.s9xrmf" --solver "$ROOT/room050-solver" --output "$OUT" --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc
"$ROOT/diagonal-venv/bin/python" -B remaster/tools/ALttP/alttp_room050_review.py --capture "$CAP/frame_room050.s9xrmf" --output "$OUT" --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc
"$ROOT/diagonal-venv/bin/python" -B remaster/tools/ALttP/alttp_stair_light_review.py --captures "$CAP" --output "$OUT/fixture-gates" --solver "$ROOT/room050-solver" --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc --fixtures build/remaster-geometry/surface-audit-room-local
git diff --check
```
Newchangesonly:frame.h,remaster.h,placement_equivalence_test.cpp,
geometry_cache_probe.cpp,room050_probe.cpp,newobstacle_assembly_test.cpp,
statue_evidence.py,componentreportobstaclecounts,thisreport. Priorparentreport,
transitions/sharedshell,diagonal/pot/stair fixes,authoredarrays,userprofile preserved.
Schema30and8statecache unchanged. No manualperstatue/roommetadata extension.

ArtifactsOUTstatue-own-evidence.json,statues-effective-finalpacket/inventory/json,
statue-prior-part-policy.*,corpus-statue-after.json,latestv2JSON/SVG/gate.json.
Smallestnextaction: parentverifybuild then comparestatueheads/middle/feet in fresh
sameprofilegameplay; remainingtruewall476-nodefree-datum needs actualsame-state
plane0capture as priorreport specifies. Wholeobstacleunknowns need specificfoot
ownership evidence, never groupingneighborfloors to fill120parts.
