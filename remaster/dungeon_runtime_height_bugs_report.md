# Room060/071 runtime height follow-up — reviewable but incomplete

**Update:** the subsequent static repair is documented in
`dungeon_stair071_static_report.md`. It resolves the pinned north001 conflict
and replaces the unjustified room071 configured50 fallback with the proved
authored24 stair relationship. The original findings below are retained as
historical before-state evidence, not the current remaining-gap statement.

Room060 own085: animation ownership repaired in runtime, not room approval.
Room071 own001/007/008: internal doorway lifecycle repaired; static stairwell
geometry remains blocked by conflicting/incomplete assembly/support evidence.
No profiles, authored arrays, Door payloads, capture068 recovery files or reports
were edited. Existing dirty shared changes were preserved. No commits.

## Pins and reproduction

Inputs: `~/Library/Application Support/Snes9x/Remaster/frame_room060_085.s9xrmf`
and `frame_room071_{001,007,008}.s9xrmf`, with each packet's OWN profile,
complete ALTPDI1 maps/attributes/VRAM, ALTPDT1 templates and terrain sidecar.
ROM SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
All four are module7/submodule0, collision0, graphics bases16384, PPU map
bases[4096,0], sizes[3,3], configured rise50. Both BGs and gameplay planes
remain independent. No current-profile substitution or compiled sidecar used.

| capture | serialized profile FNV | Link XY / plane | camera | checked BG pixel usages |
|---|---|---|---|---|
|060_085|5a53a1f5b75105d6|376,3468 / 0|256,3345|103281|
|071_001|e36c55bfeb34bcc8|683,3700 / 1|512,3592|96746|
|071_007|559857c646bb2c97|632,3894 / 0|512,3841|90338|
|071_008|559857c646bb2c97|632,3774 / 0|512,3601|96236|

Complete WRAM maps agree with PPU maps for these settled wrappers. Every visible
main/sub/scene BG identity, flip and canonical tilePixel passed camera validation.
Artifacts, exact SHA256 pins, commands and failed gates are under:
`/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/runtime-height-bugs/`.
`corpus.json` pins every available own-profile input. Per-capture subdirectories
contain ALTPPD1 solver-only wrappers (not synthetic gameplay), matching ALTPAT1,
state, graph, runtime placements, ALTPHM3 compiled map, validation-v2 JSON/SVG.

## Symptom → failure class → reusable fix

### Animated south floor-torch pedestal

BG2 cells x44,45,50,51 / y54,55. Static foot `fd86d3ed217c0d03`
has local0..8; animated cap own085 is `85dd6cce93173861`, local1..8.
The authored two-member `Floor_torch_pedastal` group instead names cap
`9943c286d5e45726`. At that phase the group matches; on other phases the
static foot was still reset to an unmatched authored obstacle (base255) while
the animated cap inherited base24 separately. This is NOT failure to notice
changed VRAM: referenced graphics already invalidate classification correctly.
The newly classified result itself lost whole-footprint support ownership.

`S9xRemasterDungeonPlacements` now recognizes full ROM floor-torch templates
0xec2/0xeca in the shared exact terminal-obstacle matcher (all H/V transforms,
priority-only overwrites, unique ownership). A partial authored identity group
cannot erase that complete ROM ownership. A complete authored footprint still
takes precedence, and competing complete authored claims veto fallback. No
hash/name/room whitelist or animation artwork equivalence is used. Authored
local pixels are never copied between identities. Missing local metadata still
rejects ROM geometry. The terminal solver accepts height2 footprints and uses
the existing complete-distinct-lower-floor + exclusively solid upper-body proof
to distinguish underpass support from the visible obstacle owner. Both feet
must independently witness one solved component/plane/base. Structural plane
bases are exported without overwriting independent actor-support plane arrays.

Own085 all eight members now have base24. Three real DMA phases reproduced
from ROM atlas provenance (themed pack0x5d + pack0x5c, current phase0) repeated
twice: all eight retained24; cached/full effective packets byte-identical.
Authored samples, normals, emission, group authoring remain unchanged.

### Internal north/south doorway reset

The publishability guard rejected every submodule except settled0, stairs8/16
and completed supertile2. Internal camera transition1 therefore stopped applying
both runtime maps, returning raw authored geometry until completion. ROM
`kDungeon_IntraRoomTrans[8]` proves phases0..7 stay in the same complete room:
prep/filter, shutter reset, scroll, landing, move out, filter, trigger shutters.
The last phase enters handler5 (ControlShutters), which also does not load rooms.
Those states now use exact live map/attribute/graphics/profile invalidation.
No retained stale map or blanket nonzero-submodule permission was added.
Partial supertile construction and unsupported stair phases still reject.

Own007 and own008 each produce byte-identical effective packets and complete
two-BG map inventories for handler1 phases0..7 plus handler5 versus settled0.
This is a controlled lifecycle ablation on pinned inputs, NOT an observed
transition trace. Animated inputs still rebuild; actual shutter writes can
invalidate ownership locally rather than reset every wall.

### Room071 static stairwell and apparent obsolete50 — NOT fixed

Northern entry art is BG1, not BG2. Own001 BG1 has 564 structural conflict
records, all594 walls unknown, support missing[52,3289] of[492,3289]. Example
graph witnesses are node2545/2546 versus support6 requiring both0 and24.
Own007/008 use a DIFFERENT authored profile: BG1 then solves586 walls, zero
unknown/conflicts, with local0..7 faces and bases16/8/0 at the north entry.
Profile differences include newly activated floor/door semantics and metadata,
not a harmless camera-only change. These edits were not reverted or propagated.

BG2 still solves834 walls with264 unknown and4 structural conflicts in007/008
(001:828/270/4). Both report two configured stair fallback constraints.
The 50-unit upper floor is the configured signed-stair fallback because BG1
exports NO accepted landing-to-landing authored support relation. It is not a
height50 profile rule. The profile also deliberately retains the unrelated
`c40518b112d0814e` local60 ceiling. No destructive rule cleanup is justified.
Southern far-wall bases50/58/66 reproduce, rather than being a debug-only tint.
The remaining shared capability is proven cross-BG stairwell assembly/landing
support transport and correct seam ownership, including high/low contacts and
partial overwrites; not replacement of every50 with24 or nearest-floor filling.

## Corpus, denominators, and gates

All233 local indoor packets inventoried:62 own-input replays,162 missing complete
evidence,9 refused stale BG identity/camera/VRAM observations. Twelve replay
summaries change; room071001/007/008 static summaries do not. Offline paired
corpus:346 entrance contexts /319 of320 rooms, room017 absent, pinned own007
metadata SHA256 `8ef2a4e57a55ba03a68dcfff8230eca980f475d83b482920a985edaa95ac3cf4`.
No switch/chest/water/door-state exhaustiveness or borrowed-context parity claim.
78 contexts change effective geometry inventories. Solved/unknown walls unchanged
89638/256672; conflicts2018→2019 (room0721→0, room0b211→13, deliberately explicit).
Unmatched authored-group cells782→12 due to ROM ownership, NOT fabricated locals.
Across5,668,864 placed BG/plane entries: missing local1,992,842 unchanged;
known-local unknown-base2,951,054→2,949,674; base+local724,968→726,348. The latter
is NOT solved geometry approval and does not silently exclude transparent art.
Support-list digests change in060/061/062 where formerly independent cap
obstacles become terminal ROM footprints (060 removes four spurious plane0 cap
support entries at x44,45,50,51/y54; visible structural base24 remains). This is
not evidence of new walkable area and does not collapse underpass planes.

Mandatory validator was run, never relaxed:

| exact capture | reachable support v2 | planes | unknown BG2 walls | exit |
|---|---|---|---|---|
|060_085|711/711, missing0|437,274|0|1 (conflict record remains)|
|071_001|87/87, missing0|0,87|270|1|
|071_007|52/52, missing0|52,0|264|1|
|071_008|52/52, missing0|52,0|264|1|

060 uses the complete exact-attribute directed reference graph;071 has NO
matching directed stair trace/complete doorway graph, so these are only
reachable-component diagnostics, NOT a substituted complete-room approval.
Runtime all-support scope for071007/008 is[492,3279] with zero missing; the
52-cell validator graph is explicitly insufficient. Each complete artifact
includes effective sources, floor bases, fallback directions, per-plane counts
and conflict/missing inventory. Full BG1 structural validation remains a gap.
Regression fixtures with own007 metadata:055 support439/439, walls1670/16,
exit1;060 support711/711, walls1030/0, exit1;061 validator exits2 because the
directional diagonal preview lacks independent ROM proof at layer1(2,38).
No room is successful, publishable, or handed back as capture-ready approval.

## Checks and smallest next work

- Nine narrow portable tests pass: animated footprint, geometry cache,
  obstacle assembly, authored obstacles, obstacle invariance, symmetric column,
  semantic cache, classification plane, placement reuse.
- Own085 three-phase real-ROM DMA replay and own007/008 lifecycle packet oracles
  pass. Profile serialization unchanged by replay.
- arm64 macOS Debug build passed; `git diff --check` passed.
- Attempted `dungeon_stair_emitter_sequence_test` on own071 failed its
  `actual offscreen fixture denominator` precondition (no offscreen sources at
  that camera); not claimed passing. Placement-equivalence utility was invoked
  as an inventory producer, not claimed as a differential assertion.
- No fresh interactive gameplay comparison, transition chronology or screenshots.

Next: instrument/reproduce complete room071 doorway + both stair transitions
with matching full maps/attributes and writer/assembly provenance; resolve the
0/24 BG1 seam witnesses and the missing cross-BG signed landing relationship.
Then rerun full graph v2 and both BG inventories before visual approval.
Capture068 Door payload recovery is separate and untouched by this task.

Reproduce tools: compile `remaster/tests/dungeon_geometry_cache_capture_probe.cpp` and
`remaster/tools/ALttP/alttp_runtime_room_heights.cpp` with C++17/O2, then use
`remaster/tools/ALttP/alttp_runtime_height_bug_audit.py` (paired executables, read-only
corpus/own profiles) and `alttp_runtime_height_bug_gates.py` (pinned ALTPPD1/
ALTPAT1 wrappers, graph scope recorded, validator JSON/SVG/map). Probe switches
`ALTTP_SUBTILE_HEIGHT_REGRESSION=1` and `ALTTP_TORCH_PHASE_REGRESSION=<3072-byte
ROM-proven DMA phases>` exercise actual effective packet paths. Binary phases
and ROM/captures remain local review artifacts, never repository source.
