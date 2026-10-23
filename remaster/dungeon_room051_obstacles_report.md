# Room051 platform border and throne floor support

## Result

Pinned `frame_room051_030.s9xrmf`: all 34 placements of the four reported border
identities now classify as obstacles and use floor base50 instead of lower-plane0.
Both complete authored throne groups (32 members) now use base50 instead of255
(unresolved), with all16 declared contacts witnessed. Authored profile bytes,
height/normal arrays and group definitions are unchanged. Actual production
EndFrame output verifies4,197 affected main/sub/scene pixel usages at offset50,
without placement-height replacement or fallback ramps.

## Reusable failure classes and repair

* Missing classification: subtype0 DC's chest-platform border had local relief
  but no semantic role. `remaster.h` recognizes the verified ROM template0xab4
  ordered perimeter for every handler dimension (n1..16, body height5..35 odd),
  including both wings and four-cell central aisle. Furniture can overwrite the
  interior, but a missing perimeter word rejects classification. No hash/room
  elevation list, profile edits or room rectangle is used. Existing obstacle
  floor constraints supply the actual supporting base.
* Uncovered contact strip: a throne's floor strip is only one tile deep, so no
  complete16x16 movement body occupies it. It nonetheless has independently
  solved authored Floor geometry at50. `frame.h` follows an interior contact
  through only its own contiguous assembly to its outside edge, then traces
  exclusively legal zero-local Floor placements at the same solved plane/base
  to ONE independently occupied component. Other objects, unknown art, walls,
  changed bases and conflicting components cannot supply support. Movement
  coverage is not expanded. Existing distinct lower-floor/body collision proof
  identifies the upper obstacle owner; Link's plane is not a tie-break.
* The independent Python authored-obstacle validator reconstructs the same
  footprint-edge and explicit-floor-path evidence, rather than trusting runtime
  obstacle offsets as support.

## Verification

`dungeon_room051_probe.cpp` checks complete pinned inputs against110,847 captured
BG identity/flip/canonical-camera samples, both complete maps,34border/32throne
denominators, effective packet pixels and rejected damaged perimeter.
`dungeon_authored_obstacle_test.cpp` adds interior-contact/narrow-strip support
and unknown-art rejection. Existing transformations, independent copies,
partial/ambiguous footprints, signed bases and plane conflicts remain covered.

Passed optimized and ASan/UBSan authored-obstacle regressions; obstacle support,
component, assembly, invariance, geometry cache and terrain lifecycle regressions;
12 Python authored-obstacle tests; arm64 Debug/Release app builds; `git diff --check`.

Corpus:346default/entrance contexts in319room IDs, bothBGs/planes with this
capture's unchanged profile. Complete cached/full maps, diagnostics and packet
comparisons pass; this is equivalence/regression scope, not all-state approval.
Artifacts: `build/room051030-corpus.json`, `build/room051030-validation/` and
temporary-root `room051030-before*` / `room051030-after*` inventories/packets.

Reproduce from the repository root (create the output directory first):
```sh
c++ -std=c++17 -O2 remaster/tests/dungeon_room051_probe.cpp -o build/room051-probe
build/room051-probe "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_030.s9xrmf" build/room051030-result
c++ -std=c++17 -O2 remaster/tools/ALttP/alttp_runtime_room_heights.cpp -o build/room051-solver
python3 -B remaster/tools/ALttP/alttp_replay_live_height_evidence.py "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_030.s9xrmf" --solver build/room051-solver --output build/room051030-validation --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc
python3 -B remaster/tests/sprite_obstacle_gate.py --capture "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_030.s9xrmf" --output build/room051030-validation --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc
```

## Mandatory complete-room v2: still fails

Lossless complete ALTPPD1 and matching ALTPAT1 reproduced from captured ALTPDI1;
ROM SHA25666871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb,
profile FNV1a64 415e910070de3684, full pins in validation/gate.json. No stair
playback was supplied/invented. Reachable plane denominators[76,0],76/76valid,
0missing/overlap; fallback10/10,0conflicts; BG2walls564solved/24unknown; one
conflicting component record; authored obstacles64/80solved,16missing,
0conflicting,24/28contacts witnessed; marked doorframes8/12solved; compiled map
0missing/8mismatches. These remaining walls/other groups/doorframes and missing
attested stair traversal prevent full Room051 height approval. The artifact
retains effective local sources, floor bases, ramps/directions and missing counts.

No interactive gameplay comparison was performed. Reopen
`build/release/Snes9x.app` (or Debug) for a fresh live comparison; existing capture geometry
is historical and is not overwritten by rebuilding.
