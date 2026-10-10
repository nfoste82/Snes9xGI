# Room061/050 and room060/047 — reviewable but incomplete

## Recurring artwork timing / eight-state cache (latest)

New opt-in captureprobe benchmark ALTTP_STAIR_RECURRING_SAMPLES: five REAL complete
VRAMstates018/025/026/032/041, OWN051fixedprofile, exacttopologyassertion, visibleBG
identity refreshed from actual sampledVRAM; fixedRGB/OAM/camera. Two independent
workloads: settled0/phase0 and stair8/phase1. Cache reset ONLY workload boundary;
5coldwarmupframes then100cyclicEndFrames, NO forcedfull/oracleinterleaving. Every
state's complete twoBGmap inventory equals firstcycle throughout recurrence.

O2 sequential result:
*settled median7.95529ms,p958.23904ms,60publications,0equivalencereuses,60exacthits.
*stair median8.08287ms,p958.34929ms,60publications,0equivalencereuses,60exacthits.
Both retain3distinct states (five captureinputs contain repeatedreferencedstates),
within existing8statebound.60publicationevents are exactstatecache restoration,
not60solves; remaining40frames reference sameactive state. Allcoldclassify/solve
occurs duringwarmup; exact recurringhits demonstrate no repeatedfullsolver work.
No staleproducer or emission freezing. About0.13msmedian difference in this run,
not controlledstatisticalregressionproof. EndFrameONLY excludesCPUfield/GPU/audio/
presentation; notlive60FPS. Prior173msoracle total must not be used FPSestimate.

Only changedfile benchmark extension in alreadydirty captureprobe,plusreport.
No frontend/appbuild changes. Parent companion analysis remains independent.
Repro:
```sh
c++ -std=c++17 -O2 remaster/dungeon_geometry_cache_capture_probe.cpp -o "$ROOT/stair-recurring"
SAMPLES="$CAP/frame_room061_018.s9xrmf;$CAP/frame_room061_025.s9xrmf;$CAP/frame_room061_026.s9xrmf;$CAP/frame_room061_032.s9xrmf;$CAP/frame_room061_041.s9xrmf"
ALTTP_STAIR_RECURRING_SAMPLES="$SAMPLES" "$ROOT/stair-recurring" "$CAP/frame_room061_051.s9xrmf" "$ROOT/stair-recurring-artifact"
git diff --check
```
Completeinput/OWNprofiledigest checks run first; no currentprofile substitution.

## APPENDED SESSION2 per-source animation flicker repair (latest)

Parent independent verification: prior-publication ablation reproduced area192
and5/5 per-source mismatches; repair restored1064/1040/1064/1040/1064 and0/5
mismatches. Room060 phase-changing24-step field/source regression passed.
Recurring five-art-input run (5warmup/100measured, no interleaved full oracle):
settled median8.034ms/p958.263, stair median8.147/p958.409; both60exact hits,
3retained states. EndFrame CPU only, not liveFPS. Release/Debug arm64 builds and
diff check passed; log `tool_123f9b0b0001YuceV4ywFZcVWF` in local tool-output.
Companion session2:692transports,all20lights/0visibleemitters;172use only192
samples, corroborating collapsed source sizes. No GPU error.459stock fallbacks
occur only before first transport(sequence950), because reused existing core
trace file arms companion during startup; not evidence of stair-time fallback.
No additional frontend repair inferred. User's running app remains untouched;
fresh live confirmation after restart is pending.

User confirms totalphaseblackout fixed but flicker remains. Exact original trace
is preserved:1200records split sample0/eventstart into[600,600],SHA256
32291e4ec8b0a26ce5586b3c99c1e311d4c84b174d5fb96202fd4b925f1116ad.
Session2:334handler8phase1,6handler8phase0,260settled0phase0;all600have20lights,
visibleemission0,collision0,plane0,scale200,original0,profilekey16831970033651525589.
Source count alone hides defect. **216frames** retain only area192 compared
1040/1064 valid animated area. First4sphere radii2.03421→0.56419(area52→4),
Z14.1629→10.1961,worldY264.54→272.5;remaining16radii2.03421→0.935603(area52→11),
worldY325.49→328.95etc. Intensity stays≈35. Firstareaedge sample7 holds mapkey/
epoch55 unchanged; repeating18dimframes/9brightframes in stair intervals. None
crosscamera viewport boundary (visiblecount0 throughout); worldcoords remove
camera motion. Three animationmapkeys; epochs advance only settled intervals.
Small intended flame variations area1040↔1064/intensity31.98↔35are NOT suppressed.

**Proved mechanism:** completed input publication remained settled-only even
after emitterphasefix. LiveVRAM flames animate duringstairphase1; retained producer
hashes remain previousanimation. Offscreen emitter's exactcurrentproducer check
CORRECTLY rejects changed flameparts; static emittingfixtureparts survive and
produce20tiny/sunken spheres. This is not safe animation-equivalence/stale-source
permission. Need current exact geometry/producer identity through proven complete
in-room state, not weakened hashchecks, heldlights/default elevations.

Implemented shared publication boundary `S9xRemasterDungeonHeightInputsPublishable`
accepting verifiedmodule7/submodule8or16/B0phase0or1 completeinputs. Existing exact
maps/attrs/VRAM/profile/support invalidation, geometry-equivalent proof, bounded
8statecache and fullfallback now operate during in-room climb. No geometry solver
rule or authoredsourcevalues changed; planechanges correctlyinvalidate/re-solve.
Unsupportedphases/roomconstruction reject. Entrygroundedanchors remain settled
only. Sourceproducerchecks unchanged; animatedactual metadata consumed perframe.

Evidence replay: own051profileSHA9e5dd3b1fbea2076cd52ec13fc512b7f9bbc43bf36937ea8c51bbe708bb3ec9e,
complete recordedVRAM samples061018/025/026/032/041, maps/attrs exactequal.
These are REALcapturedart states, fixedown051researchprofile, NOT chronological
traceVRAM reconstruction or substitutedoldprofiles. Priorpublication ablation
ALTTP_TEST_PRIOR_STAIR_ARTWORK_GATE reproducesarea192in5/5alternatingstates,
5per-source mismatches versus independent exactcurrentsettledfullsolve. Repaired
area1064/1040/1064/1040/1064;0/5mismatches in EACHsourceXYZ/radius/intensity/RGB.
This matches live216area-collapseframes class. Latest052ispacket-only(noOWN
complete/profile sidecars),cannot serve immutablegeometryreplacement.

Tests:O2andASan/UBSanrealanimationoracles pass,061all3stairs and060planechanging
phase1source/fieldtests pass,geometrycachetestpass,diffcheckpass.600prior-session
testnowstopsatfirst600ratherthanconsumingappendedsession;originalfileuntouched.
Corpus346contexts/319rooms,bothBGs/planes/defaultsyntheticanimation:339reuses/
7fallbacks,completeoutputsidenticalfull. Own051profilepin;medianreuse0.203625/
full20.8641ms(empty8x8corpusNOTFPS). Gatesrerun060exit1,050/051exit2sameindependent
BG2(2,38)proofgap. No approval; offscreen/otherunknownwallgaps remain.
Animation5statecostbefore40.44ms/after173.86msTOTAL measuredstairEndFrame in oracle
run forcibly fullsettledsolvingbetweenstates; coldexactpublicationmorework, NOT
steady-statecacheperformance/live60FPS. Existingbounded8statecache retained;
repeatedrealstates noweligiblecache ratherthan artificially staleproducers.

Changedhere:remaster.h publicationboundary,captureprobeanimationoracle,
sequenceplanechange/append handling,newcoreonlytraceanalyzer,thisreport.
No parentfrontend/torchheight/profile/schema edits. App remainsrunning; no
launch/termination/build. Parent independentchecks/builds andfreshlive trace
must confirm residualflicker; legitimateauthoredflamepower variation remains.

Commands:
```sh
python3 -B tools/ALttP/alttp_stair_trace_sources.py "$CAP/stair061-trace-01.jsonl" --output "$OUT/appended-source-sessions.json"
c++ -std=c++17 -O2 -DALTTP_TEST_PRIOR_STAIR_ARTWORK_GATE remaster/dungeon_geometry_cache_capture_probe.cpp -o "$ROOT/stair-animation-before"
c++ -std=c++17 -O2 remaster/dungeon_geometry_cache_capture_probe.cpp -o "$ROOT/stair-animation-after"
SAMPLES="$CAP/frame_room061_026.s9xrmf;$CAP/frame_room061_018.s9xrmf;$CAP/frame_room061_032.s9xrmf;$CAP/frame_room061_025.s9xrmf;$CAP/frame_room061_041.s9xrmf"
ALTTP_STAIR_ANIMATION_SAMPLES="$SAMPLES" "$ROOT/stair-animation-before" "$CAP/frame_room061_051.s9xrmf" "$ROOT/stair-animation-before-artifact"
ALTTP_STAIR_ANIMATION_SAMPLES="$SAMPLES" "$ROOT/stair-animation-after" "$CAP/frame_room061_051.s9xrmf" "$ROOT/stair-animation-after-artifact"
# Compile same probe -O1 -g -fsanitize=address,undefined for sanitizer oracle.
# Reuse corpus+mandatory gate commands below with freshly compiledsolvers/probes.
git diff --check
```
Artifacts:$OUT/appended-source-sessions.jsonlosslesscoreworldsourceinventory,
stair-art-corpus.json,live*/gate.json. Priortraceandcaptureimmutablepins retained.

## LIVE TRACE ROOT CAUSE + shared phase repair (latest)

Parent independently inspected2400 companion presentation events:1167 transport,
1167 successful GPU completions,66 slot drops,zero stock fallbacks/GPU errors.
348 transport events had zero lights/emitters/samples;819 had20/0/512. Companion
window extends beyond core600-frame window, so those denominators are separate.
This corroborates source publication loss rather than a renderer failure.
Parent compiled/replayed the prior gate and repaired production lifecycle:249
blackout frames before,0 after across600 records;061 all3 assemblies,060 plane
change and emitter unit tests passed. Final arm64 Release and Debug builds plus
diff check passed. Build log `tool_123ed36c70017KV6D4tfiF5AiJ` in local tool-output.
User's running app was left open; restart is required to load the repair.

Exact `$CAP/stair061-trace-01.jsonl`:600records,SHA256
7be89647834b8293d649afc7eb72721e8776458b79243e7aaafa8992b499b080.
Core only; parent owns companion analysis.346settled0/phase0frames and5stair8/
phase0frames retain20lights; **249stair8/phase1frames drop ALLlights**. All600
visible-emitting counts0. Module7,room97,collision0,plane0,transitionflags0,
sceneempty,256×224,scale200,original0,bounces0,profilegeometrykey16831970033651525589
invariant. Five stairstarts, four completed traversals, last17climbframes before
bounded stop. Dropping occurs samples1,109,257,404,583; lights return57,169,313,464.
Each drop preserves prior epoch/key; first0→1 also exactsameLink760/3175 and
camera640/3073. Source disappearance is not a camera/cache/profile/plane event.
With original0/bounces0/no visible emitters, source20→0 removes entire nonHUD
lighting supply. Static051 was settled, so could not reveal this temporal defect.

Verified variables.h maps WRAM$B0 to **subsubmodule_index**, not universal
construction flag. Module07_08/10 phase0initializes and incrementsB0;phase1climbs
until timer0 then resetsB0/submodule. Neither constructs maps/attributes. First
repair admitted8/16 but still required!constructionPhase; legitimateB0=1 rejected.
Prior controlled tests wrongly treated1as construction independent of handler.
Shared lifecycle now accepts ONLY phases0/1 in8/16; settled0stillrequiresphase0,
unknown2+and othermodules/transitions reject. All producer/base/currentVRAM/ROM
checks remain; no room whitelist,height/profile/schema/cache changes. Existing
field name retained compatibility but predicate documents module-specificB0.

Actual600lifecycle/Link/plane/camera sequence replay through productionEndFrame:
baseline ALTTP_TEST_PRIOR_STAIR_PHASE_GATE reproduces249/249climbblackoutframes;
repaired0/249drops. Own051completeprofile/maps/art fixed EXPLICITLY: trace lacks
perframeVRAM/profiletext and traceprofilekey differs own051; this is actual
lifecycle failure-class replay, not exact chronological geometry reenactment.
Gate is independent of artwork, and original trace has exactsource loss atB0edge.
72controlled061steps/all3stairs and24room060steps/planechanging16nowincludephase1;
exactsourceheight,mapepoch,incremental/fullfieldoracles pass. Unsupportedphase2/
roomtransition negatives,ASan/UBSan actual600replay,emitterunit,geometrycache pass.
O2sequence+fieldoracle0611806.89ms/72,060550.763ms/24,NOT FPSbenchmark. No core
geometry algorithm touched: prior346context/319room differential and v2failures
remain applicable. Prior ROI upper-floor zero illumination is a separate facing/
height condition, not explanation of249complete temporal blackout frames.

Changed here:offscreen_emitters.h,offscreen_emitters_test.cpp,
dungeon_stair_emitter_sequence_test.cpp,this report. Parent frontend/concurrent
report additions preserved. App stays running; no builds/launch/termination.
Parent independently builds/checks then fresh matchingliveverification required.
060literalstepcadence not traced here; same verifiedhandler16phaseclass repaired.

Repro baseline/repaired (ROOT/CAP/OUT defined below):
```sh
TRACE="$CAP/stair061-trace-01.jsonl"
c++ -std=c++17 -O2 -DALTTP_TEST_PRIOR_STAIR_PHASE_GATE remaster/dungeon_stair_emitter_sequence_test.cpp -o "$ROOT/stair-phase-before"
ALTTP_STAIR_REPLAY_TRACE="$TRACE" "$ROOT/stair-phase-before" "$CAP/frame_room061_051.s9xrmf" "$ROOT/stair051-phase-before"
c++ -std=c++17 -O2 remaster/dungeon_stair_emitter_sequence_test.cpp -o "$ROOT/stair-phase-after"
ALTTP_STAIR_REPLAY_TRACE="$TRACE" "$ROOT/stair-phase-after" "$CAP/frame_room061_051.s9xrmf" "$ROOT/stair051-phase-after"
"$ROOT/stair-phase-after" "$CAP/frame_room060_047.s9xrmf" "$ROOT/stair047-phase-after"
c++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer remaster/dungeon_stair_emitter_sequence_test.cpp -o "$ROOT/stair-phase-san"
ALTTP_STAIR_REPLAY_TRACE="$TRACE" "$ROOT/stair-phase-san" "$CAP/frame_room061_051.s9xrmf" "$ROOT/stair051-phase-san"
c++ -std=c++17 -O2 remaster/offscreen_emitters_test.cpp -o "$ROOT/stair-phase-unit"
"$ROOT/stair-phase-unit"
c++ -std=c++17 -O2 remaster/dungeon_geometry_cache_test.cpp -o "$ROOT/stair-phase-cache"
"$ROOT/stair-phase-cache"
git diff --check
```
No extra linkdependencies:fixtureincludescaptureprobe/header-onlyfields. Separate
ALTTP_STAIR_REPLAY_TRACE from S9X_REMASTER_STAIR_LIGHT_TRACE freshpatharming test.

## Parent actual051 verification and diagnostic build

Parent independently compiled/passed actual051 production regression (112580
matched samples,20 sources,512 sphere samples,0 visible emitters) and GPU ROI
replay on051/050/060047. Confirmed051 upper-flat5659pixels have zero direct
radiance, lower-flat21640lit pixels and stair501lit pixels. This is not a
temporal symptom repair. Frontend review found no stair-specific presentation
gate; added opt-in companion events for stock fallback, slot drops, transport
counts and GPU completion/errors in `macosx/mac-render.mm`. Companion starts
only when core creates its trace at first stair handler; no GPU readback or
scheduling changes. Final arm64 Release/Debug builds and diff check passed after
separating trace counter increment from stream expression (new warning fixed).
Final build log: `tool_123e4dc54001147bTdnDeenDvU` in local tool-output directory.
Remaining blocker: matching chronological traversal trace. No new source-height
or shader repair established; user-reported disappearance remains unresolved.

## Trace arming update (latest)

CORE trace now waits for FIRST module7/submodule8or16 before creating/opening its
file.600completed frames then record (~10seconds at60FPS); first line explicitly
`event=stair_trace_start,sample=0`, subsequent lines event=frame. Changing path
resets arming/window. Navigation time does not consume it. Use a UNIQUE nonexistent
path: an old file would falsely signal frontend arming. Parent's frontend companion
must skip until core path exists, then record up to2400events to
`$S9X_REMASTER_STAIR_LIGHT_TRACE.presentation.jsonl` (stock_fallback,slot_drop,
transport counts,gpu_complete/error; no readback). Enabled-only file existence
checks/trace I/O; disabled runtime remains free of trace filesystem work.

User manually quits their app first, then launches verified rebuilt executable
directly from Terminal to inherit environment; this worker did not launch it:
```sh
TRACE="$HOME/Library/Application Support/Snes9x/Remaster/stair-light-$(date +%Y%m%d-%H%M%S).jsonl"
S9X_REMASTER_STAIR_LIGHT_TRACE="$TRACE" \
 "$PWD/build/release/Snes9x.app/Contents/MacOS/Snes9x"
```
Use actual parent-built executable path if different. Navigate normally, then
traverse problem stairs immediately on first stair entry and reproduce within
10seconds; collect BOTH unique core file and `.presentation.jsonl`. Capturing each
room separately with a fresh manual restart/path avoids spending061window in060.
Trace costs perturb performance; not an FPS benchmark. Source/hash/ROI caveats
below remain; frontend counts/completion alone do not prove GPU ROI brightness.

Regression verified fresh-path settled own replay does NOT create file, first
handler frame DOES create explicitstart, and subsequent72steps061/all3stairs,
24steps060 retain exact source/maps/fields. Commands (fresh filenames required):
```sh
c++ -std=c++17 -O2 remaster/dungeon_stair_emitter_sequence_test.cpp -o "$ROOT/stair-armed-test"
S9X_REMASTER_STAIR_LIGHT_TRACE="$OUT/armed061-new.jsonl" "$ROOT/stair-armed-test" \
 "$CAP/frame_room061_051.s9xrmf" "$ROOT/stair051-armed"
S9X_REMASTER_STAIR_LIGHT_TRACE="$OUT/armed060-new.jsonl" "$ROOT/stair-armed-test" \
 "$CAP/frame_room060_047.s9xrmf" "$ROOT/stair047-armed"
git diff --check
```
Worker edits here only core trace arming,sequence regression,report; no further
runtime geometry/source/shader repair or macOS frontend edits. Both runs passed.

## ROI/elevation versus occlusion diagnosis (supersedes global count interpretation)

Extended standalone probe with settings-only experiment and unoccluded diagnostic:
`ALTTP_STAIR_FAST_ROI=1 ALTTP_STAIR_SETTINGS051=1`, optionally
`ALTTP_STAIR_UNOCCLUDED=1`. Settings experiment ONLY sets original contribution0,
bounces0 (OWN051 values); preserves every packet's own source/profile geometry,
normal/height/scale/emission and camera. This is NOT same-state gameplay evidence.
Unoccluded diagnostic zeroes coverage for block/ray traversal; no source or
receiver height/normals altered. Results:

|capture/ROI|pixels|lit normal/unoccluded|sum radiance normal/unoccluded|
|---|---:|---:|---:|
|051 upper authored floor|5659|0/0|0/0|
|051 lower authored floor|23636|21640/23636|119.467/124.843|
|051 stair treads|768|501/656|0.825783/1.29236|
|050 upper authored floor|8137|0/0|0/0|
|050 lower authored floor|24119|22911/24119|356.854/464.538|
|050 stair treads|1536|1041/1235|2.01809/2.75704|
|060047 upper authored floor|8687|8458/8687|51.5171/51.9705|
|060047 stair treads|256|218/256|0.049731/1.37284|

061 upper-floor normals meanNZ≈1,Z18.8235..19.6078: **entire relevant upper
floor is black**, even unoccluded, because current physical lights cannot reach
it from above. It was misleading to infer absence of meaningful blackout from
global36913 lit receivers. Those are lower floor/walls/stair lower treads. Own051
composite image reproduces upper-platform darkness without a presentation gate.
This proves an elevation/facing cutoff with current source geometry, not an
occlusion/cache bug. No negative upper heights or inverted normals observed.
060047 stairs differ: real occlusion reduces tread sum96.4%; upper-floor sum
changes<1%. Temporal per-step oscillation may occur at moving actor/tread ray
boundaries but one packet does not establish the cadence or incorrect blocker.

Actual emitter-placement inventory (`ALTTP_STAIR_SOURCE_EVIDENCE=1` capture probe):
all44BG2 source cells in051 have solvedbase0,plane0base0,plane1unknown; no local
height preview replacement. Upper brazier cells(28/29/34/35,32..34) authored
mean20/16.1579/13. Lower torch cells(26/27/36/37,40/41,44/45,48/49,52/53)
authoredmean7. All emissionDepth0. This matches physical source heights:
visible local and offscreen local share scale200/255; no unit mismatch,
double floor base, flattened preview, active actor plane contamination detected.
These fixtures are room-local y256+ on solved LOWERfloor0, not upperfloor24 at
y56..80. Entry absent. Current source support therefore is internally consistent;
reassigning them to upperfloor or raising local20above24 without asset/assembly
evidence would invent geometry. Current local-height metadata/rise interplay can
make the upper room genuinely unlit under original0/bounces0; actual source
authoring intent needs review before claiming a metadata repair is admissible.

Camera representation: offscreen code excludes EVERY geometrically in-viewport
emitting pixel, even hidden ones, to avoid reintroducing occluded fixtures. Remaining
pixels become area-equivalent spheres, possibly combined by reciprocal asset links.
Visible pixels use their authored emission shape/source endpoints; offscreen spheres
use weighted centroid, not per-pixel emitter quadrature. This is a representation
approximation, so camera can change near-boundary energy/occlusion. Actual050/051
have differentcamera/settings/art: cannot isolate representation alone. No shared
coordinate or visibility defect proved from these pins. Need a same-profile live
camera/stair sequence to prove a harmful boundary discontinuity before replacing
physical sphere contract. No lights/unknown geometry flooded or whitelisted.

Implemented bounded opt-in CORE chronology `S9X_REMASTER_STAIR_LIGHT_TRACE=/path`:
first600completed EndFrame samples per path, room/module/submodule/construction/
transition/collision,LinkXY/Z/plane,camera,input/map/profilegeometry keys/epoch,
scene/dimensions/settings, visible emission count and actual physical XYZR/intensity.
Disabled path does no extra field scan/I/O. Changed pathname resets bounded trace;
same path never grows beyond600per process. Explicitly diagnostic, not production
perframe logging. Trace is opt-in and can affect timings; do not benchmark60FPS
with it. Tested actual051 production replay and diffcheck. Parent can implement
frontend trace using same sample order/room/camera: Draw path chosen/fallback
reason,resource slot/generation,field/source signatures,count/ranges,command
completion and ROI readback nearLink/treads/upperfloor. Minimum needed: actual
matched60..120consecutive frames before/during/after center and060stair, OWNprofile
fixed, inputs/field/source hashes + snapshots on first ROI radiance discontinuity.
No mac-render.mm changes by this worker. Pair by room/camera/sampleorder; core
frame API currently has no persisted presentation frame serial, so parent's
frontend may add shared optional diagnostic serial if required.

ROI/trace reproduction:
```sh
# Compile Metal probe with command below; ONLY controlled diagnostic settings.
ALTTP_STAIR_FAST_ROI=1 ALTTP_STAIR_SETTINGS051=1 "$ROOT/stair-metal" \
 "$CAP/frame_room061_051.s9xrmf" "$CAP/frame_room061_050.s9xrmf" "$CAP/frame_room060_047.s9xrmf"
ALTTP_STAIR_FAST_ROI=1 ALTTP_STAIR_SETTINGS051=1 ALTTP_STAIR_UNOCCLUDED=1 "$ROOT/stair-metal" \
 "$CAP/frame_room061_051.s9xrmf" "$CAP/frame_room061_050.s9xrmf" "$CAP/frame_room060_047.s9xrmf"
c++ -std=c++17 -O2 remaster/dungeon_geometry_cache_capture_probe.cpp -o "$ROOT/stair-sources"
ALTTP_STAIR_SOURCE_EVIDENCE=1 "$ROOT/stair-sources" "$CAP/frame_room061_051.s9xrmf" "$ROOT/stair051-sources"
c++ -std=c++17 -O2 remaster/dungeon_stair_actual_state_test.cpp -o "$ROOT/stair-trace-test"
S9X_REMASTER_STAIR_LIGHT_TRACE="$OUT/trace-test.jsonl" "$ROOT/stair-trace-test" "$CAP/frame_room061_051.s9xrmf" "$ROOT/stair051-trace"
```
ROI classification: fullviewport placed BG2 authoredFloor and NZ>.99,upperZ>18,
lowerabsZ<.01; treads/stair roles. Link-box ROI is fixed screen112..143,88..143,
so not exact assembled actor ownership, excluded from definitive claims above.
Future trace must use actual actor support/room placement coordinates rather than
mistaking a rectangle for geometry. Complete-room approval still blocked as below.

## Actual captured Metal transport replay (latest)

Implemented `tools/remaster_stair_transport_probe.mm`, standalone Foundation/Metal
executable, no app target/launch. Runtime-compiles exact `macosx/shaders.metal` and
its REMASTER_REFERENCE_VISIBILITY variant. Fields use OWN packet, production main
scene choice, HUD preservation; mesh is produced from exact field-cache samples,
then exact production emissionDepth/omnidirectional assignment and unauthored
mesh-normal restoration. RGB555 conversion matches mac-render. Sphere light
ABI48bytes/uniform128bytes, source firstSample offsets/adaptive counts, prepared
records/patches, dummy empty buffers, visibility blocks, emission seed, direct
serial/batched, partial reduction and composite bindings match production.

**48 actual GPU cases passed**:051/050/060047 × accelerated/reference visibility
× adaptive/full spheres × clustered/nonclustered × serial/batched. No shader
blackout or nonfinite radiance reproduced. Exact051 offscreen-only20sources:

|variant|lit receivers >1e-6|max radiance|sum(maxRGB)|nonzero composite pixels|
|---|---:|---:|---:|---:|
|adaptive512sphere samples|36913|0.230103|208.613|37747|
|full2560sphere samples|37005|0.230103|208.862|37837|

051 has0patches/0prepared records; production-sized dummy patch/record/sample
buffers remain valid. Clustered and nonclustered direct results identical;
accelerated and pixel-reference visibility identical. Maximum serial/batch
half-float discrepancy6.10352e-5adaptive,3.05176e-5full. Adaptive sphere sampling
is not an on/off bug here. Offscreen visibility endpoints tested through real
ray traversal; no unaccelerated-only light restoration observed.

050 adaptive:20spheres/832samples,36nonclustered records or12patches/12records;
lit38197nonclustered/38190clustered,peaks17.4375/17.0156,sum1014.53/997.451.
060047 adaptive:6spheres/320samples,1380records or24patches/96records;
lit34869/34475,peaks1.03223/1.66309,sum388.855/368.723. These are authored patch
aggregation differences, not equality expectations across nonempty clustering.
Serial/batched and visibility reference comparisons within each exact source
layout pass, maximum difference0.00012207. Full sphere variants remain illuminated.

**Important appearance distinction:** OWN051 original_scene_contribution=0 and
indirect_bounces=0; OWN050 contribution0.0279523004,bounces0.051 composite is
visibly quite dark on the raised platform despite NONZERO transport; captured
sourceZ14.1629 lies below upper floor24×200/255=18.8235, so its upper +Z surfaces
cannot receive that source directly. This is physically consistent with supplied
profiles/solved bases and NOT evidence authorizing guessed source elevations,
default ambient, bridge flooding, flipping normals, or bypassing occlusion.
There are no retained chronological camera/animation/state frames in051's
capture_animation tracks;050 uses its own different profile and camera, not an
admissible same-settings A/B. The image shows a dark-but-illuminated outcome;
do not equate lit-count success with user's expected live brightness being fixed.

Artifacts `$ROOT/stair-light-review/metal-transport-final.txt` (48cases),
`frame_room{061_051,061_050,060_047}.s9xrmf-{gpu-composite,stock}.{ppm,png}`.
PNG conversions use existing diagonal-venv/Pillow. Actual GPU timing includes
visibility/seed/prepare/direct/reduce/composite, excludes CPU field prep/library
compile/pipeline creation. Adaptive production clustered/batched05115.734ms,
05019.947ms,06015.256ms, single cold-case samples, NOT steady-state/live60FPS.
No FPS promise and no change to adaptive/cache policy. Full/reference variants
are deliberately costly correctness oracles (up to~724ms), not runtime settings.

No shared shader defect established, so **no shader/source-height speculative
repair**. Changed file this stage is only standalone probe + report. Parent's
independent live eligibility/resource-slot tracing remains necessary to explain
literal on/off behavior. This worker has now tested actual downstream GPU
transport, not merely source vector presence; no transport blackout demonstrated.
Remaining general evidence gap for differing expected illumination: actual same-
profile chronological frame/state/light/field/presentation output, then prove
fixture ownership/emission-height semantics if source-base error is alleged.
Complete-room v2 failures documented below remain separate approval blockers.

Repro (repo root, existing output directory):
```sh
xcrun clang++ -std=c++17 -O2 -fobjc-arc tools/remaster_stair_transport_probe.mm \
 -framework Foundation -framework Metal -o "$ROOT/stair-metal"
ALTTP_STAIR_METAL_RESULTS="$OUT/metal-transport-repeat.txt" \
ALTTP_STAIR_METAL_IMAGES="$OUT" "$ROOT/stair-metal" \
 "$CAP/frame_room061_051.s9xrmf" "$CAP/frame_room061_050.s9xrmf" \
 "$CAP/frame_room060_047.s9xrmf"
git diff --check
```
No shader/metallib build dependency: production source compiled by Metal API
at runtime; Foundation,Metal,system SIMD + repo header-only field/mesh/patch/sphere
and batch helpers. Optional images/results directories must already exist. Probe
throws on zero radiance/output,nonfinite values or serial/batched/reference/empty
patch disagreement>0.001. Does not test indirect bounces because all three OWN
captures request0; does not claim in-flight slot equivalence/live presentation.

## Follow-up own061/051 — first repair did NOT resolve live symptom

User's failed live confirmation supersedes any suggestion of symptom completion.
Actual051 regression demonstrates **no core source disappearance in the captured
state**. Its FIRST representative frame is settled submodule0, collision0,
plane0,Link760/3196,scroll640/3089,previous0,epoch232,same map cache key as050.
Packet schema30,SCN1 absent,20 valid offscreen physical lights,**zero visible
emitting pixels**,55,267known receivers,512adaptive sphere samples. OWN profile
FNV52593f80479c4b04,SHA256
9e5dd3b1fbea2076cd52ec13fc512b7f9bbc43bf36937ea8c51bbe708bb3ec9e.
All six immutable SHA pins/state/validator command in
`$ROOT/stair-light-review/live051/gate.json`.

050 versus051 complete maps/attributes/templates and both effective plane/visible
bases are byte/value-identical.279VRAM bytes differ; OWN051 profile is different,
and is used ONLY for051, never substituted into050. SourceZ14.1629 and5.4902
matches captured/recomputed exactly;050 first sources13.844 reflect its own
animation/local-height state.061051 pixels112580 match map/VRAM/flips/camera.
Actual packet's Link animation tracks are observations/assets, not per-frame
WRAM/geometry/light packets. No extra chronological animation/state companions
exist for051. Running120frame animation capture retains FIRST frame/state, not
the specific middle frame user saw; do not relabel051 as handler8 by intent.
Terrain attests actual LinkZ=-1 through existing reader; no entry anchor consumed.

**Why the first fix failed to address this captured state:** its emitter gate
already admitted submodule0. Captured051 and production EndFrame both have all20
sources even BEFORE any further change. Settled050/051 ordinary-scene eligibility
and complete support remain intact. Fixed-artwork controlled handler sequences
proved a separate real lifecycle omission, not the zero-visible-emitter live
failure. There is no evidence that the user deployed incorrectly. Neither capture
has held lamp; no guessed continuous stair support/height correction justified.

New `dungeon_stair_actual_state_test.cpp` exercises exact actual051:
captured versus production20sources EXACT XYZ/radius/intensity/RGB; retained
versus full height/emission/participation/surface fields; zero visible emission,
nonzero physical sources and known receiver denominators. O2 and ASan/UBSan pass.
This is baseline reproduction of **the actual transport inputs**, NOT a passing
visual before/after symptom repair. It proves source publication survives, and
identifies the zero-visible-emission transport class the parent's non-overlapping
frontend/GPU investigation must cover. No room-specific elevation/source defaults
or frontend edits made here. Parent owns source-transport/renderer repair if its
independent investigation establishes the downstream failing condition; this
worker cannot truthfully claim an additional core symptom repair from these inputs.

Read-only renderer review found ordinary scene selected when SCN1 absent; sphere
lights are copied independently of visible emitter count, batch calculation
includes sphere samples, shader sphere loop is separate from authored samples.
That source reading is NOT GPU execution proof. Parent must test actual051
offscreen-only direct/indirect and clustered/nonclustered pipelines, resource
bindings/accumulation/eligibility; **do not stop at checking lights vector size**.
No mac-render.mm edits by this worker. No opt-in per-frame trace added without
a demonstrated temporal core defect; precise WRAM/source/renderer chronology
remains required if the captured downstream failure cannot be reproduced.

Evidence repair: additive capture JSON fields `module`, `construction_phase`,
`room_transition_flags` in remaster.h, and replay probe consumes them when present.
Old051 lacks these fields; module7/construction0 are legacy adapter assumptions,
NOT independently retained historical pins. Publishable_inputs=true/submodule0
are retained. Schema30/ALTPDA1/ALTPDI1 unchanged. Future captures retain actual
lifecycle values rather than assuming them. No solve/cache geometry change.

Latest corpus capture denominator101:35complete,32exact cache/full equivalences,
3unchanged stale rejections,66missing sidecars (one additional packet-only capture
arrived concurrently). Mandatory gates rerun:060exit1;050/051exit2 same independent
partial-diagonal source proof gap atBG2(2,38). No room approval. Prior346context
geometry differential remains applicable: no geometry algorithm changed in this
follow-up. Fresh app/GPU validation remains parent's responsibility.

Exact follow-up commands (ROOT/CAP/OUT as below; no app build required):
```sh
c++ -std=c++17 -O2 remaster/dungeon_stair_actual_state_test.cpp -o "$ROOT/stair-actual051"
"$ROOT/stair-actual051" "$CAP/frame_room061_051.s9xrmf" "$ROOT/stair051-actual"
c++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer remaster/dungeon_stair_actual_state_test.cpp -o "$ROOT/stair-actual051-san"
"$ROOT/stair-actual051-san" "$CAP/frame_room061_051.s9xrmf" "$ROOT/stair051-san"
# Test reuses capture probe as fixture and includes field cache + sphere sampling;
# header-only core, no additional link libraries.
c++ -std=c++17 -O2 remaster/dungeon_geometry_cache_capture_probe.cpp -o "$ROOT/stair-capture"
c++ -std=c++17 -O2 -DALTTP_TEST_DISABLE_GEOMETRY_REUSE remaster/dungeon_geometry_cache_capture_probe.cpp -o "$ROOT/stair-full"
python3 -B remaster/dungeon_geometry_cache_captures.py --captures "$CAP" --enabled "$ROOT/stair-capture" --full "$ROOT/stair-full" --output "$OUT"
"$ROOT/diagonal-venv/bin/python" -B tools/ALttP/alttp_stair_light_review.py --captures "$CAP" --output "$OUT" --solver "$ROOT/stair-solver" --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc --fixtures build/remaster-geometry/surface-audit-room-local
git diff --check
```

Additional follow-up changed files: new actual-state regression, additive evidence
fields in remaster.h, optional-field replay consumption in capture probe,051
included in review script, this report. User profile/artwork and parent frontend
work preserved. Smallest next action: independently reproduce actual051 transport
on GPU and fix the demonstrated shared downstream failure; chronological trace
only if actual packet inputs cannot reproduce it.

Implemented a shared, evidence-backed **physical emitter lifecycle repair**.
This is not complete room approval or proof that every reported oscillating
frame is fixed. Parent independent verification and app builds passed (below).
No app launch/termination, profile/artwork/frontend/schema edits, staging or commit.

## Parent verification

Independently compiled and passed the optimized offscreen emitter regression and
the own-profile production sequence test on both supplied captures:061 matched
112587 BG usages and retained20 sources across72 controlled steps/all3 assemblies;
060 matched92723 usages and retained6 sources across24 controlled steps including
the plane change. Both sequence tests also passed exact full/incremental field,
height-map/cache-epoch and transition/construction rejection assertions.
Arm64 Release and Debug builds and `git diff --check` passed. Build log:
`/Users/nic.foster/.local/share/opencode/tool-output/tool_123d25ded001FN8fWi9mBKP4bQ`.
Products: `build/release/Snes9x.app`, `build/debug/Snes9x.app`.
Existing deployment-target, deprecated-API and narrowing warnings remain.
Interactive stair traversal still needs confirmation; controlled sequences keep
the captured viewport/artwork fixed and do not prove the user's literal cadence.

## Immutable diagnosis

Both OWN profile files have serialized FNV `c379a5e0c96413ac`, SHA256
`e1017b49f23d653d07c84e823162cafbd3419d647a1b16c99958058e031f0744`.
ROM SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Rise50, coordinate scale200, both graphics bases16384, map bases[4096,0],
sizes[3,3], collision0, settled submodule0, module7, grounded attested.
Exact SHA256 for packet, OWN profile, full input, templates, state and terrain:
`$OUT/stair-light-review/live{050,047}/gate.json`. Complete8192 WRAM/PPU words
agree; templates match verified ROM. No current profile substituted.

*061/050*: room97,previous96,Link689/3214,plane0,scroll569/3108,
cache2723313893699392151,epoch1189. Production checks112587 matching BG pixel
identity/flip/canonical camera usages. Stairs(30,11),(10,20),(50,20),north-high,
same-plane. Packet has20 explicit **offscreen physical sources**, only36 visible
emitting main pixels,0 emitting sub pixels. SourceZ13.844 and5.4902.
*060/047*: room96,previous97,Link376/3363,plane1,scroll256/3257,
cache1721422858130588721,epoch1108.92323 matching BG usages. Stair(46,42),
south-high,plane-changing.6 offscreen sources,Z26.8473/24.4796,345 visible
emitting main pixels,0 sub. Physical fixtures use solved visible BG placement
base plus unchanged authored local height and half emission depth, **not Link's
plane or entry elevation**. Solved bases include24 despite configured fallback
rise50: authored geometry constraints supersede fallback; do not rescale ramps.

Both packets have schema30, NO SCN1 lamp-free scene and NO held lamp among their
explicit sources. The ordinary stock BG2 scene is present. Therefore these
packets do not support attributing this symptom to held-lamp height/recovery or
changing the dark-room scene eligibility in gfx.cpp. `$ROOT/stair-light` prints
exact lights, emitting counts, and composed height samples without substitution.

### Proven failure class / shared repair

`S9xRemasterAppendOffscreenRoomLights` previously rejected **all nonzero
submodules**. Actual USA handlers Module07_08_NorthIntraRoomStairs(828f0c) and
Module07_10_SouthIntraRoomStairs(828f88) only move/animate Link, camera and plane;
they do not reconstruct maps/attributes. Thus every stair-handler frame dropped
all offscreen sources even with valid retained geometry. Center061 particularly
depends on offscreen sources. Visible pixel emitters follow a separate path and
do not share this gate, explaining why visibility/camera context can expose this
class differently without a room-specific stair rule.

New shared `S9xRemasterRoomEmitterLifecycle` admits settled0 and proven in-room
handlers8/16 ONLY, requires module7/constructionPhase0. Existing ROM, dimensions,
collision, room/map, current VRAM identity, placement producer hash/flips and solved
base checks remain intact. Room transitions/construction, unknown ownership/base,
changed animation identity still reject. No neighbor fill/default elevation.
Sources remain recomputed from current data, not carried stale through a light
cache. Geometry solve publication remains settled-only. Eight-state cache intact.

Existing reference playback backs lifecycle distinction:060 round trip contains
118 handler16 frames;061 center and west contain118 handler8 frames each. East
script has0 handler frames (prior sprite-route gap). Those are reference/default
states, not chronological traces of the new Snes9x packets.

### Independent gaps (not silently "fixed")

Static captures are after/away from active handlers. They cannot establish the
literal per-step ON/OFF cadence, changing palette/emission, camera visibility,
plane timing, or entry continuity across the user's actual sequence. Entry support
is absent in both own pins and cannot be recreated from entry position. Existing
entry/transition tests pass; controlled intra-room sequence consumes no entry
anchor and advances no solve epoch. Actor placement still uses current plane
support; no new stair support interpolation/actor-foot geometry was invented.
Held lamp currently uses a plane floor cell and retains supplied floor when that
cell is unknown; this is a separate unproved continuous stair-source capability,
not evidence for changing these non-lamp captures. Record actual per-frame WRAM
module/submodule,Link XY/Z/plane, construction flags, maps/attributes/VRAM/profile,
source list and renderer state to close the residual live symptom attribution.

## Tests / coverage / gates

* `offscreen_emitters_test`: both handlers/both actor planes preserve transformed
  source height; overwritten identity, unknown base and construction reject;
  existing camera/mirrored BG, signed base, animation/extinguished/combination tests.
* New production own-input sequence test: all3 room061 assemblies,72 controlled
  handler steps; room060 assembly,24 steps including plane change. All20/6 sources
  retain EXACT XY/Z/intensity; full map inventory/epoch unchanged; no invented
  transition/entry anchor. Room-transition/construction negatives pass. Incremental
  field height/emission equals forced-full extraction at every step. These are
  handler-backed controlled replays with fixed viewport/art, NOT live movement.
* O2 sequence+two field oracles costs1872.05ms/72 and546.849ms/24; not EndFrame or
  liveFPS cost. ASan/UBSan passes the same final72/24 cases without diagnostics.
* Geometry-cache, entry-lifecycle and transition-cache regressions pass.
*99 available060/061 packets:34complete,31exact cache-enabled/full equivalences,
  3preexisting stale own inputs rejected,65missing complete sidecars. All own pins
  and rejection diagnostics in `$OUT/stair-light-review/manifest.json`.
*346contexts/319roomIDs, bothBGs/planes/default states and synthetic animation:
  339reuses/7full fallbacks; complete maps/diagnostics/production packets equal
  forced-full. Unchanged residual unknown walls256552,conflicts1288,
  missing movement[529,0]. Source changes here do not alter settled-room geometry.
  Missing320throom and switch/water/door states remain unsupported denominators.
* Stationary own050 O2 EndFrame40 measured+10warmup:median8.24858,p958.63946ms,
  0epochs,0reuses,0state hits. Timings overlapped independent portable tests;
  not controlled A/B, complete-frame budget or60FPS proof. Corpus empty8x8 reuse
  median0.207125/full21.1877ms likewise not gameplay timing.
* Mandatory independent v2 exact collision/full graph pins:060exit1,
  711/711supports,planes[437,274],overlap24,1017solved/9unknown walls,
  compiled8missing/46mismatches.061exit2: independent partial diagonal proof gap
  at BG2(2,38), unchanged prior action defect. Solver061 supports1570/1570,
  956solved/2unknown walls,0structural conflicts. Do not claim v2 success or hand
  either room back as approved for capture. Generated HM3 map is solver-derived
  review representation, not authority over runtime split/region geometry.
* `git diff --check` passed. No matching interactive GPU/gameplay comparison.

## Changed files / reproducible commands

Worker changes only: `offscreen_emitters.h`, `offscreen_emitters_test.cpp`, new
`dungeon_stair_emitter_sequence_test.cpp`, `dungeon_stair_light_probe.cpp`, this
report, `tools/ALttP/alttp_stair_light_review.py`. One-line explicit `return 0`
in already-dirty capture probe is necessary when its main is reused as a fixture
function (ordinary main's implicit success no longer applies); other concurrent
probe edits untouched. All prior060/061/062 fixes, authored arrays and schema30
preserved. Parent frontend/current profile changes were neither read as evidence
nor overwritten.

Repository root; output directory already created:
```sh
ROOT=/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode
CAP="$HOME/Library/Application Support/Snes9x/Remaster"
OUT="$ROOT/stair-light-review"
c++ -std=c++17 -O2 remaster/offscreen_emitters_test.cpp -o "$ROOT/stair-emitters-test"
"$ROOT/stair-emitters-test"
c++ -std=c++17 -O2 remaster/dungeon_stair_emitter_sequence_test.cpp -o "$ROOT/stair-sequence"
"$ROOT/stair-sequence" "$CAP/frame_room061_050.s9xrmf" "$ROOT/stair050-after"
"$ROOT/stair-sequence" "$CAP/frame_room060_047.s9xrmf" "$ROOT/stair047-after"
# Sanitizer dependency is same translation unit, no external linked library:
c++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer remaster/dungeon_stair_emitter_sequence_test.cpp -o "$ROOT/stair-sequence-san"
"$ROOT/stair-sequence-san" "$CAP/frame_room061_050.s9xrmf" "$ROOT/stair050-san"
"$ROOT/stair-sequence-san" "$CAP/frame_room060_047.s9xrmf" "$ROOT/stair047-san"
c++ -std=c++17 -O2 remaster/dungeon_geometry_cache_capture_probe.cpp -o "$ROOT/stair-capture"
c++ -std=c++17 -O2 -DALTTP_TEST_DISABLE_GEOMETRY_REUSE remaster/dungeon_geometry_cache_capture_probe.cpp -o "$ROOT/stair-full"
python3 -B remaster/dungeon_geometry_cache_captures.py --captures "$CAP" --enabled "$ROOT/stair-capture" --full "$ROOT/stair-full" --output "$OUT"
c++ -std=c++17 -O2 remaster/dungeon_geometry_cache_probe.cpp -o "$ROOT/stair-corpus"
python3 -B remaster/dungeon_geometry_cache_corpus.py --corpus "$ROOT/diagonal-sets-priority" --profile "$CAP/frame_room061_050.s9xrmf.profile.toml" --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc --probe "$ROOT/stair-corpus" --output "$OUT/corpus.json"
c++ -std=c++17 -O2 tools/ALttP/alttp_runtime_room_heights.cpp -o "$ROOT/stair-solver"
# Python environment needs Pillow (independent validator import dependency).
"$ROOT/diagonal-venv/bin/python" -B tools/ALttP/alttp_stair_light_review.py --captures "$CAP" --output "$OUT" --solver "$ROOT/stair-solver" --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc --fixtures build/remaster-geometry/surface-audit-room-local
# Exact validator commands/exits preserved in each live*/gate.json, runner does
# not turn those failures into approval.
for test in dungeon_geometry_cache_test dungeon_transition_cache_test dungeon_entry_lifecycle_test; do
 c++ -std=c++17 -O2 "remaster/$test.cpp" -o "$ROOT/stair-$test" && "$ROOT/stair-$test"
done
ALTTP_CACHE_BENCHMARK=40 ALTTP_CACHE_ONLY_MODE=0 "$ROOT/stair-capture" "$CAP/frame_room061_050.s9xrmf" "$ROOT/stair050-benchmark"
git diff --check
```

Smallest next action: parent independently verifies shared gate/builds, then
obtains matching chronological gameplay emitter/submodule/scene evidence to
confirm remaining060 cadence and061 center/working-stair contrast. Complete-room
approval separately needs the independent diagonal proof and residual wall/map
coverage repaired; this lifecycle fix must not conceal them.
