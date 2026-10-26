# Dungeon transition height lifetime (2026-10-22)

## Runtime repair

Outgoing geometry is frozen at ordinary Module07/02 departure, including both
BG maps, local previews, fallback directions, plane bases and final tile words.
The USA loader overwrites `$A2` with the destination at phase2. Previously the
outgoing lifetime check treated that legitimate scratch update as an unrelated
jump and discarded the snapshot. The retained transition now owns predecessor
identity; rendering restores that identity even while live `$A2` names current.
Unrelated destinations, profile/ROM/session changes, and viewport retirement
still reject/expire the snapshot. Entry-support observation follows the same
loader-scratch rule.

The game's collision builder processes only64 word pairs per frame, then object,
door and barrier overrides. Its completion can lag camera scrolling. The old
publication gate waited for that builder. `dungeon_transition_attributes.h`
reproduces its complete read-only output from the destination's final WRAM maps,
tile-attribute LUT, object/stair/door tables and state, without changing emulated
RAM. `gfx.cpp` prepares and retains this output at phase2 (room loading complete,
before camera phase8), allowing the existing complete-room solver to publish
early. It is retained once because the real builder consumes torch/chest scratch
counters. Unsupported/corrupt inventories reject, rather than claiming partial
collision is complete. The old complete-live-input path remains available.

This does not guess geometry, change authored relief/normals/profile data, or
apply destination geometry to source pixels. Incremental PPU uploads still need
exact placed-word/hash/flip qualification. No changes to stock game execution.

## Evidence

- `dungeon_departure_geometry_test.cpp`: production EndFrame, distinct both-BG
  bases/local ramps, plane+collision changes, source-ID still current, direct
  phase2 `$A2=current` update, early destination solve, outgoing retirement,
  resets/profile/ROM invalidation and wrong producer rejection.
- `dungeon_transition_attributes_test.cpp`: independent basic flips, both-plane
  stairs, indexed blockers, door spans, RAM immutability and invalid inventories;
  optimized and ASan/UBSan runs pass.
- Original USA reference-engine builder differential: **320 rooms ×8192 cells =
  2,621,440 cells, zero mismatches**, input RAM unchanged. This enumerates room
  construction using the local entrance4/theme4 engine context, not every
  progression state or graphics context. The input uses ROM SHA256
  `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
- Both user captures `frame_room051_040.s9xrmf` and
  `frame_room061_071.s9xrmf`, their own pinned profiles and complete sidecars:
  production EndFrame controlled departure ablation retains **exact per-pixel
  background bases/local heights and complete both-BG geometry** through phases
  0..7 with overwritten collision, changed actor plane and `$A2=current`.
  This is not fresh captured gameplay; the packets retain the first animation
  frame/state and cannot attest every intermediate emulated transition state.
- arm64 Debug macOS app builds successfully. Quick portable suite includes the
  new builder test. No interactive traversal has been performed.

## Complete-room validation v2: not room-height approval

Full matching ALTPPD1/ALTPAT1, pinned state/profile and runtime inventories are
in `build/transition-validation-{051,061}/`. Gate manifests retain all supplied
packet/input/profile/template/terrain/sprite hashes. No directed stair playback
was supplied; no edges were invented.

Room051: reachable denominators `[0,360]`,360/360 valid support,4/4 valid fallback
placements,0 fallback conflicts,564 solved/54 unknown walls,4 structural
conflicts,4 compiled-map missing entries and54 compiled-map mismatches. Local
source/effective-height/floor-base/fallback/plane inventories are in
`validation-v2.json`; gate exit1. These are existing geometry limitations, not
fixed by transition timing.

Room061: reachable denominators `[296,0]`; validator exits2 because a directional
diagonal preview at BG2 cell(2,38) lacks independent ROM proof. No coverage pass
is claimed and the validator was not weakened. Required next general capability
is independent diagonal provenance validation, alongside unresolved placement
ownership/geometry in051. Corpus builder equality proves collision preparation,
not correct height coverage for all placed tiles or all rooms/states.

## Reproduction

```sh
python3 remaster/tools/run_remaster_tests.py --suite quick --output build/transition-height-verified-tests
python3 remaster/tools/ALttP/alttp_build_transition_attributes_probe.py --engine build/remaster-geometry/room-preview-engine --output build/transition-attributes-probe
# Run the resulting binary with the reference engine as its working directory:
# <absolute binary> 0x61 <absolute output.ppm> 4
c++ -std=c++17 -O2 -pthread remaster/tests/dungeon_geometry_cache_capture_probe.cpp -o build/transition-capture-probe
ALTTP_DEPARTURE_HEIGHT_REGRESSION=1 build/transition-capture-probe "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_040.s9xrmf" build/transition-051
ALTTP_DEPARTURE_HEIGHT_REGRESSION=1 build/transition-capture-probe "$HOME/Library/Application Support/Snes9x/Remaster/frame_room061_071.s9xrmf" build/transition-061
```

Relaunch `build/debug/Snes9x.app` for fresh traversal review. Historical captures
are not rewritten. Release app was not rebuilt in this task.
