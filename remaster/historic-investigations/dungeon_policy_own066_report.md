# Per-member obstacle policy and own061066 northwest repair

## Parent verification and final products

Independent native authored-obstacle and own066 cardinal-local diagonal
regressions passed, including NW/SW actual packet checks, malformed/overwritten
rejection, preserved arrays and mixed fallback sources. Python independent
obstacle9 and diagonal5 tests passed. Inspector feedback now describes authored
floor-only offsets versus generated bottom0/top7 plusZ*8, with Z tooltip.
Arm64 Release/Debug builds and diff check passed after both repairs:
`/Users/nic.foster/.local/share/opencode/tool-output/tool_12bed1580001eNgtrXQ68kT4b0`.
Products `build/release/Snes9x.app` / `build/debug/Snes9x.app`.
Running process untouched; fresh user visual confirmation remains pending.

Verdict: **reviewable but incomplete**, not room approval. No user profile/capture
or parent frontend/helper edits. Frame30/profile22 and exact eight-state cache
remain unchanged. Parent performs arm64 Debug build and fresh interactive review.

## Current policy (supersedes older obstacle reports)

Every matched group uses ONE evidenced component/plane/floor base, with every
explicit support contact required. Per-member `hasHeight` controls composition:

* Authored local array: effective=`floorBase + authoredPixels`. MemberZ is NOT
  applied; the entire authored assembly is raised only by its shared floor base.
* No authored array: effective=`floorBase + memberZ*8 + generatedLocalRamp`.
  Ramp is displayed bottom0/top7, canonicalized through the already-composed
  member/globalV flip; H has no effect on the vertical gradient.
* Mixed groups use these policies independently per member. Generated arrays live
  only in runtime placement previews, never profile assets. Unknown/conflicting
  contacts and incomplete/ambiguous/overlapping footprints still reject.

Matching no longer requires local metadata. `producerGeneratedObstacle` and
runtime-only previews distinguish authored versus generated sources. Offsets for
authored members remain the floor datum; Z offsets apply only to fallback members.
Independent Python proof reconstructs the same policy and compares actual
frame-applied64pixel arrays. Compiler still preserves missing local metadata;
reporting it missing intrinsically is correct even when runtime fallback solves
the placement. `hasHeight` changes expire geometry/profile/entry/cache evidence.

Native regression covers mixed sources, allH/V transformations, signed floor/Z,
authored immutability, missing-source matching, explicit contact unknown/conflict,
eight-state cached/full inventories and hasHeight-only invalidation. ASan/UBSan
passes. Python authored proof9 tests, diagonal5, validator24 and compiler10 pass.
Existing statue/column/obstacle/pot/scroll/stair/transition/cache tests pass.

Mixed temporary own057 example:48/48 members, zero missing/conflicts/Python-native
mismatches. Research own-profile corpus:152 packets,43 replayed,102 missing
sidecar unsupported,7 stale refusals;672/672 members solved,0 missing/conflicts/
mismatches, source hashes immutable. Approved runtime fallback replaces the prior
missing-local blocker; it does not assert those hashes now have authored metadata.

Reference mixed-profile corpus:346contexts/319rooms,200members/168solved,
32unresolved,50contacts/50witnessed,4assembly conflicts. All cache/full maps,
evidence/diagnostics and packet comparisons equal. Explicit plane/component
ambiguity remains unresolved even when both contacts exist.

## Own066 root cause and generalized repair

Pinned immutable packet `frame_room061_066.s9xrmf`, settled module7/submodule0,
room061, plane0, rise50, graphicsBG1/BG2base16384, scroll[512,3083],
Link[597,3190]. ROM SHA256
66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb.

Packet SHA256 ecc257325298ce8ea290a6df31fe82b35bd8770abd78e6be023aba6d3dbf47d8;
profile caf4ca7d1f51d5900d484c447645b7e17dd769dcff5393e0e91a69b901e0bcfa;
complete input adea8c3547ce6a5620e331ceea19d098cc22453e7d82707837739f55d4a46876;
templates be6a8a9c5b436772f96700973307ec81fc31843e06118e0e567b658e59bc5ddd.
Own-input replay verifies112589 map/identity/flip/camera pixels and own serialized
profile pin. ROM template sidecar agrees exactly with verified ROM table.

Symptom: northwest diagonal explicitSE assets were classified Unknown and their
placements had base255 despite adjacent upper support24. Root cause is cardinal
cross-section calibration, NOT proof that the southwest repair caused a new
regression: own048/057/066 all have the same0..7 top cardinal metadata. One HIGH
cardinal top template is explicitly authored local0..7, while its lower two bands
and the other axis retain authored absolute40..47/32..39/24..31. Requiring all six
bands to have absolute offsets rejected every diagonal calibration.

Fix: both axes' LOWER TWO bands must independently agree on the exact signed
datum/unit slope. Top bands must be either exact absolute continuation or explicit
Wall local0..7 ramp. No arbitrary/missing/malformed top is accepted. Exact final
ROM stepped footprint still proves diagonal band identity/ownership; overwrites,
opening roles and ambiguous bands reject. The affine diagonal owns its oblique
cardinal seam for both composite and short ROM shells; the constant-tangent
cardinal reciprocal cannot contribute a contradictory one-pixel extrapolation.
No profile arrays, room/hash whitelist or nearest-floor rules were added.

Own066 after:1570/1570supports,[1570,0]plane denominators,zero missing,
zero structural conflicts;1010solved BG2walls/2unknown. The previously omitted
diagonal placements are now real effective heights: NW base0 plus authored-section
preview24..48, not debug recoloring or a floor default. Actual produced packet
NW gradient assertions pass over>500visible pixels. Own048 southwest actual
seven-band gradient regression and own057/066 cached/full replay pass; BG1 retains
6unknown walls. A direct historical variable-diagonal test remains failing its
old zero-unknown-wall assertion; do not confuse that command with passing
own048 actual packet gradient checks or waive room-wide residuals.

Python calibration mirrors independent proof. Fixed an existing variable-shadow
bug passing a footprint set instead of dispatch ownership mapping to partial
semantic proof. Required whole-room gate still exits2 at preexisting southwest
BG2(2,38) missing independent diagonal provenance. No available writer attestation
was invented: exact final ROM words support short diagonal identity, while
full-room dispatch/writer proof for the partial southwest remains a publication
blocker. Own066 new authored groups additionally have184required/96solved,
24unmatched members; remaining unsupported groups are explicit.

Reference own066-profile corpus346/346contexts in319rooms,zero unsupported inputs
or refusals, immutable profile;1802group members/664solved/794unmatched,
284contacts/226witnessed,16assembly conflicts. Every cache/full comparison equal.
Reference scope is default/entrance graphics states, not exhaustive dynamic states.

## Commands / artifacts

ROOT=/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode

```sh
c++ -std=c++17 -O2 remaster/tests/dungeon_authored_obstacle_test.cpp -o "$ROOT/authored-obstacle-test"
"$ROOT/authored-obstacle-test"
c++ -std=c++17 -O2 remaster/tests/dungeon_cardinal_local_diagonal_test.cpp -o "$ROOT/own066-diagonal-test"
"$ROOT/own066-diagonal-test" "/Users/nic.foster/Library/Application Support/Snes9x/Remaster/frame_room061_066.s9xrmf"
python3 -B -m unittest discover -s remaster/tools/ALttP -p test_authored_obstacles.py
python3 -B -m unittest discover -s remaster/tools/ALttP -p test_diagonal_preview.py
python3 -B remaster/tools/ALttP/alttp_validate_obstacle_live.py \
  "/Users/nic.foster/Library/Application Support/Snes9x/Remaster/frame_room061_066.s9xrmf" \
  --solver "$ROOT/authored-obstacle-solver" \
  --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc \
  --fixture-graph build/remaster-geometry/surface-audit-room-local/room-061-doorway-graph.json \
  --output "$ROOT/own066-review"
```

Artifacts `$ROOT/own066-review/`: before/after066/048/057 packets, inventories and
JSON; fresh live complete input/runtime/profile pins; gate.log (exit2), full
compiled map/live doorway graph; reference346.json, mixed-reference346.json;
policy-corpus/corpus.json and per-capture independent proofs. Mixed example
`$ROOT/authored-obstacle-review/mixed061/` includes runtime JSON/map,
independent-proof.json and gate.log (same missing diagonal proof). Generated
geometry remains review-only. git diff--check passes. Fresh live screenshot
comparison and zero-exit room gate are still required before capture approval.
