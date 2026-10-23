# Room051 `_071`: runtime geometry investigation

**Verdict: reviewable but incomplete.** No placement heights or authored arrays
were changed. The independent Tile Locations publication repair is not a height
repair. The runtime-height problems below remain open rather than receiving a
hash-wide `+8` patch.

## Exact reproduction

Input: Application Support `Remaster/frame_room051_071.s9xrmf` and its own
`.profile.toml`, `.dungeon.json`, `.dungeon-input.bin`, `.dungeon-templates.bin`,
`.terrain-input.bin`, and `.sprite-input.bin` companions. ROM SHA256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`;
serialized profile FNV1a64 `c4651736b3e5651a`; room `0x051`, predecessor `0x061`,
settled module7/submodule0, collision mode0, Link `(844,2993)` plane0,
scroll `(724,2833)`, both graphics bases `0x4000`, configured rise50.
Production replay matched 147,451 BG pixel usages against complete final maps,
VRAM identities, mirrors and pixel coordinates. Both full WRAM/PPU maps match.

Artifacts: ignored `build/room051-runtime-followup/` contains production replay,
`071-before-placements.tsv`, complete-room gate and corpus accounting. The new
`remaster/tests/dungeon_room051_runtime_probe.cpp` is an observational audit
wrapper around the existing production EndFrame replay, not a geometry generator.

Reproduce from repository root (local captured inputs required):

```sh
c++ -std=c++17 -O2 remaster/tests/dungeon_room051_runtime_probe.cpp \
  -o build/room051-runtime-followup/verified-probe
build/room051-runtime-followup/verified-probe \
  "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_071.s9xrmf" \
  build/room051-runtime-followup/verified
```

## Failure classes, not missing asset arrays

All six reported hashes have authored local sources in this pinned profile.
Complete placed usage accounting (both BGs, including offscreen cells):

| Hash suffix | BG | Placements | Local range | Solved placement bases |
|---|---|---:|---|---:|
| `99863ac4c1bb8e68` | BG1 | 2 | 12..17 | 0 |
| `76a7931735f4b282` | BG1 | 2 | 13..15 | 0 |
| `9214aaf974182610` | BG1 | 58 | 0..7 | 0 |
| `a3b3accbe0e35dca` | BG1 | 2 | 0..7 | 0 |
| `9836d0b8491c70b9` | BG1 | 10 | 0..7 | 0 |
| `9eb947628ab00ab3` | BG1 | 2 | 0..7 | 0 |

1. **Structural graph contradiction.** All 288 BG1 classified walls have unknown
   placement bases. There are 419 structural conflict events (not 419 unique
   placements). At `(13,53)` the clipped wall `a19ae88181451d61` has local0..5;
   `(14,53)` is ordinary `d00ecd4b62335cb1` local0..7. The accepted wall
   continuation requires the latter base to be8 below the former. The floor-art
   contact at `(12,53)` (`4d5247551edca28e`, local0) requires its base to be5
   above `(13,53)`. The logged -8 versus -5 are competing node bases for
   `(13,53)` relative to a temporary graph datum, not alternative edge deltas.
   The trace does not establish the complete alternate route assigning the
   neighboring wall's value. The discrepancy recurs along the west/east lower
   perimeter. Clipped cap/face ownership and exterior floor-art contact
   interpretation are hypotheses, not proven causes. Complete ownership evidence
   is needed to distinguish the correct relationships;
   suppressing contradictions or normalizing the graph would guess heights.
   Reported middle-row `0..7` walls cannot receive an evidence-backed `+8` until
   this shared cap/face/support ownership class is resolved.
2. **Stair sources vs placement support are separate.** The four reported center
   placements at x30..33,y40 classify as Stair with intact authored arrays.
   They have no solved placement base. The consumer explicitly preserves Stair
   authored absolute ramps at offset0; absence of a runtime support diagnostic is
   not absence of local height. A general stair-assembly placement-base model is
   needed before applying these ramps at a nonzero lower landing. Do not infer
   the landing datum from the center hash or entry position.
3. **Door dispatch ownership is missing.** South jambs `(30,56)`/`(33,56)` have
   authored Door masks but `doorAssembly=0`, `doorTemplateOffset=0`. The final
   BG1 door's two inner bands match a mirrored ordinary source, but its outer
   band has been replaced (`a8b5/a8b6`, etc., rather than that source's
   `a82c/a82d`). Complete-template recognition correctly refuses ownership from
   those surviving inner bands alone. Implement dispatch/overwrite provenance,
   not a jamb hash whitelist or nearest-floor attachment. The 58 instances of
   `9214...` remain classified Wall, illustrating why the same reported asset
   name must not become a global Door/height rule.

## Complete-room validation v2

`alttp_replay_live_height_evidence.py` created a lossless solver-only ALTPPD1
wrapper and matching ALTPAT1 arrays from the complete exact capture inputs. The
existing complete gate generated a compiled ALTPHM3 map and ran
`alttp_validate_room_heights.py`; command, SHA256 pins and state are retained in
`gate/gate.json`. **Validator exits1, not approved.**

- Reachable support: 886/886 on observed plane0; plane1 denominator0.
- Immediate fallback: 26/26. Plane overlap in this doorway graph:0.
- BG2 classified walls:564 solved,0 unknown. Production BG1:0/288 solved.
- BG1 structural conflicts:419; BG2 structural conflicts:16.
- Whole-room authored-floor inventory independently exposes BG1 missing counts
  [812/922,3782/3782] by plane; BG2 [0/942,0/882]. These are *not* the reachable
  doorway denominator and cannot be combined into a solved-room claim.
- Gate additionally rejects 16 doorframe ownership evidence records
  (`missing_intact_rom_door_template`). Exact live ROM writer
  provenance is absent. No reference playback stair edges were transferred:
  collision digests differ from the available reference fixture. Plane1 and
  stair-transition traversal are therefore explicitly unsupported in this graph,
  even though the runtime separately retains per-plane arrays.
- Compiled-map checking also retains two missing/mismatched support claims at
  `(30,39)` and `(33,39)`: expected24 versus unknown255. Passing the reachable
  support count alone must not hide those map failures.

No fresh interactive gameplay comparison was performed. Original packet art is
used only for identity/camera checks; it is not a successful geometry review.
Next smallest general action: reproduce ordered lower-perimeter cap/face and
south-door overwrites in the verified ROM writer trace, then add region-aware
contacts and regression cases for both mirrors, split BGs and overwritten bands.

## Available capture corpus (no height changes)

Enumerated105 complete capture bundles under Application Support and `build/`.
Production replay accepted35 own-profile pins:26 Room051 and9 Room061 states.
The other70 were explicitly rejected at the existing serialized-profile pin
check (45 Room051,10 Room061,6 Room012,5 Room002,2 Room041,1 each Room022/011).
They are unsupported for this audit, not silently replayed with today's profile;
historical serializer/profile-version compatibility needs separate investigation.
State bundles may duplicate geometry and are not105 independent rooms.

Across accepted states, classified walls total BG1 12,150 with7,542 unknown;
BG2 25,590 with246 unknown. Reported-identity usage/missing-placement-base counts:
`9214...`3578/1508, `9836...`872/260, `a3b3...`124/52,
`9eb9...`70/70, each center stair hash106/106. Stair unknown-base counts do not
mean absent authored ramps (see above). No before/after increase is claimed:
runtime heights were not changed. `corpus-summary.json` preserves every path,
error and denominator; TSVs preserve both complete BG inventories for accepted
bundles. This corpus is not all320 rooms or exhaustive switches/doors/planes.

Parent verification independently recompiled the observational probe with `-O2`
and replayed exact071: `verified.log` again reports147451 matched pixel usages,
BG1 0solved/288unknown/419conflicts and BG2 564solved/0unknown/16conflicts.
The current canonical profile still matches the pinned companion SHA256; no
authored arrays were modified.
The exact recorded complete-room validator command was independently rerun and
again exited1; output is `gate/parent-validator.log`. This is confirmation of the
open coverage/evidence failures, not a passing height gate.
ASan/UBSan exact071 production replay also completes without sanitizer findings
(`verified-sanitized.log`); unresolved geometry counts remain unchanged.
The final curated full+Metal suite passes35/35
(`build/room051071-final-tests/20261023T205833Z-q4ehba1j/results.json`).
Debug/Release app builds pass. Neither result overrides failing complete-room v2.
