# Room050 torch animation metadata — reviewable but incomplete

Metadata-first slice: no additional solver/frontend/direct/clustering changes.
User profile changes were read fresh and an exact pre-edit snapshot retained.
The tracked `remaster/profiles/ALttP/alttp-profile.toml` was edited using apply_patch.

## Proven family and reviewed authoring

Supported ROM SHA25666871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb.
ROM-matched `DecompressAnimatedDungeonTiles` loads background pack5c, then
interleaves its16-tile phase blocks with the room-themed pack. `misc.c` cycles
three0x400-byte DMA phases; NMI writes them at VRAM WORD3b00/byte7600. The
ROM-bound atlas records all decoded pack sources. Families below are identical
canonical slots `slot+16*phase`, not artwork similarity or broad profile groups.

| Slot | Three canonical asset hashes (v1:4bpp prefix omitted) | Applied geometry |
|---|---|---|
|6|9c36635bd77c6108, a8d1d0722ff1bda3, 34eb3798eedfa0fc|existing39..32 Y ramp retained; Wall/south activation aligned|
|7|713a84c159e10e36, e3ce293317632ce5, a5f3fec8d608599a|current user east0..7 ramp from713a aligned|
|8|4fbd3931fd51d954, 98b436e4f1eca7a9, e73d7df8a528afeb|current user local0..7 Y ramp from98b aligned|
|9|c5487cf307fcd5b2, 4e10c08b32d55949, cc05dbdf62879545|current user east0..7 ramp fromc548 aligned; torch cap roleWall|
|10|6e6cccc12f4f4109, 657478a2462983a8, 3226a483780043fe|current user local0..7 Y ramp from6574 aligned|
|11|0286d2de151fd423, f6d77e342fed74e8, a819ba7614ee77d0|current user east0..7 ramp from0286 aligned; torch cap roleWall|

18 unique variants now covered;16 assets changed,2 new geometry-only records.
10 height arrays changed/added (7 existing replacements,1 previously absent
height,2 new records). No change to core flame e41e81c4f0f14d5f or
fe630bf28976e706, which are NOT members of these pack5c animation families.

Differing existing variants were investigated before replacement: slot8/10
had old39..32/31..24 absolute-band ramps on phases0/2 versus the user's newly
authored phase1 local0..7. Slot7/9/11 phase1 had Y ramps while reviewed phase0
seeds had canonical X ramps. Their actual ROM DMA addresses share unchanged
placement flip bits, so no H/V conversion could justify those axis differences.
Slot6's consistent intentionally authored39..32 heights were preserved. Two
Door roles on slots9/11 refer to ROM wall-torch cap words, not door dispatch;
they were aligned to Wall under the user's explicit wall-mounted fixture contract.

Every existing emission byte, depth, omnidirectionality field, combine link,
normal array, occlusion array and unrelated field is identical to pre-edit
snapshot. Newly added records contain geometry ONLY: no fabricated emission or
normals. `alttp_torch_metadata_summary.py` independently checks this invariant.

The editor's existing Apply Height to Animation copies height/sampling only.
Observed `capture_animation_*` tracks take precedence; broad semantic groups are
not sufficient positional proof. This slice leaves editor code untouched and
adds an offline ROM-proven geometry proposal/inventory helper.

## Actual phase replay / validation

Newest capture008 lacks complete sidecars; inspected packet still shows the
phase0 side/north variants.007 is newest complete pinned room input, own profile
FNV8ce20b3a13ee3191. Its full animation DMA bytes match atlas-derived themed
pack5d/torch pack5c phase1 exactly. All three phases were reconstructed from
actual decoded ROM packs; themed slot16 identity probes uniquely identify the
graphics context. These are exact ROM-phase research replays on immutable007
maps/planes, not new Snes9x gameplay captures or synthetic recolored art.

Before metadata: fixture missing cells4/0/8, source unknown0/0/4, geometry changes
across phases. After: missing fixture/source bases0 on all three phases, identical
displayed effective arrays on ALL16 torch assembly cells (1024 samples per phase),
runtime structural conflicts0 and unknown walls485 each phase. Canonical flip
sampling remains placement-specific. Offscreen spheres4/4/6 after versus4/4/2
before. Phase2 has more spheres because existing emission masks/combine links
are intentionally retained; equal brightness/identical emitter counts are not
claimed. The metadata/profile remains immutable during solve.

Mandatory v2 ran on matching007 complete ALTPPD1/ALTPAT1 wrapper, both completed
planes, local seed[371,131], selected updated profile pinned into replay manifest,
verified ROM and compiledALTPHM3. Explicit research-profile substitution is
recorded, not disguised as capture-pinned original metadata. Supports376/376,
fallbacks4/4, missing placements0, component conflicts0, compiled missing0,
mismatches0; walls439 solved/485 unknown. Gate exits1. Broader runtime required
support[0,3564], missing[0,0]. No room approval or handoff for capture is claimed.

Corpus asset/activation audit:181 room packets,55 complete-input graphics states
in041/050/060/061/062,126 unsupported missing complete inputs/context.36 observed
canonical slot usages across both BGs: missing height/Wall activation34 ->0;
different canonical height usages14 ->0. This is metadata inventory, not all
room-height validation or matching packet parity. No320-room complete-state
coverage claim. Atlas/proven phase denominator6 slots x3 phases =18 assets.

Tests:3 animation-metadata regressions (18-member coverage, protected-field
invariance, geometry-only additions),5 existing wall-torch evidence checks,
three-phase C++ exact solver/source replay, strict C++ profile parser/serialization
through that replay, and diff-check pass. No app build needed for profile-only
authoring; no extra solver repair was introduced.

## Artifacts / reload / remaining gaps

Preapproved temp artifacts: `torch-profile-before.toml`, `torch-variants-before.json`,
`torch-variants-after.json`, `torch-metadata-changes.json`, `torch-three-phases.bin`,
`torch-metadata-corpus-before.json`, `torch-metadata-corpus-after.json`, and
`torch-metadata-gate/` (manifest/full room/planes/graph/runtime/map/v2 JSON+SVG).

Reload the tracked profile through View > Load Remaster Profile after saving or
discarding any stale editor copy. Do not save the old in-memory profile over the
new file. Profile activation/cache invalidation occurs at next frame boundary;
no executable build is required for these metadata edits. Runtime repair from
the preceding slice must already be integrated for mounted effective geometry.

Remaining blockers:485 unknown room050 walls, incomplete door-top/local ownership
and arbitrary door-region support, full-room offscreen occluders, fresh gameplay
animation/camera comparison. This turn intentionally resolves missing animation
metadata FIRST; it does not claim to repair top-wall/door-top geometry.
