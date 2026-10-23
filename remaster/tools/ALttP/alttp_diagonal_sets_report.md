# Dungeon default-state diagonal sets — reviewable but incomplete

**Independent ALL-SE denominator:** `alttp_diagonal_semantic_gap_report.md`
supersedes discovery-only completeness wording: all70 wall/SE annotations are
diagonals,58intrinsic complete and12required unresolved gaps after another24
exact ROM conversion assets. Unknown families are gaps, never excluded.

**2026-10-08 LOW-family follow-up:** the southwest omission is real, not a
reload issue. Ten proved LOW identities now have intrinsic height/normal
arrays; the HIGH20 and approved references are preserved byte-exact. The old
LOW exclusion and “zero remaining eligible updates” below describe the earlier
HIGH-only pass, not the current discovery rule. See
`alttp_low_diagonal_authoring_report.md` for exact IDs, topology, before/after
capture029 evidence, corpus denominators and current failing validation gates.

2026-10-08. **Confirmed intrinsic authoring completed; ALL-game/ALL-state
discovery and complete-room approval are blocked by missing evidence.**

## Scope, inputs and discovery

Supported USA ROM SHA256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Renderer v21 SHA256
`b8d55f7b621e3c72a496fedb65b03737981e5b206615f927c6041fffee6525cb`.
Semantic source SHA256
`be4630cfeb8f0c6ca9489d4e37c917354b43e86b86cb271a383235bd2a5a2579`.
Baseline approved profile SHA256
`d991dc00da1bed28ad4db6e925711efa8f59c8bed84ead693028f2055cac92ef`;
authored profile SHA256
`32ac03c9f5f5ad7f14886f1c7232a15682238c122f00b90d9dad25b1e6ec4ee8`.

Attempted all **320 dungeon IDs**, all own entrances (not just the first),
otherwise the renderer's explicitly recorded borrowed entrance. **347 contexts,
346 completed, 319 room IDs, 213 borrowed contexts**. Room `0x017` failed to
settle (arrived in `0x027`, submodule7). Complete BG1/BG2 maps, checked ALTPRV1
final words, completed ALTPAT1 attributes and state sidecars are preserved for
each completed context. **2,834,432 placed cells** examined across both BGs.
The attribute arrays are available, but this scan does not solve movement or
support for those 319 rooms. Configured floor rise is not used to infer any
intrinsic band; fixture validation retains rise50 and independent planes.

ROM template source file offset `0x1b52` (SNES80:9b52). Subtype0 handlers
`09..14` hex use templates `0x420..0x48e`: five-word columns, minimum six
repetitions, stepped (+1,-1) or (+1,+1). `15..20` hex are **LOW** handlers,
templates `0x498..0x506`, writing both BGs; no 24-rise transfer to this family.
Other cardinal, corner, rail, stair, trim and overworld families are outside
this four-band authoring rule, not silently assumed equivalent.

Actual dispatch origin/length/phase/ordinal comes from the ROM-derived atlas
and ALTPRV1 raw dispatch. Accept a placed band when final writer AND word
match the reproduced footprint, or the narrowly proved priority-only flank
operation below preserves uniquely owned artwork. V flips reverse band mapping;
canonical artwork stays southeast-facing, then the existing runtime transforms
pixel sampling and normal components with placement H/V bits. No similarity
search, hash grouping guess, room rectangle, runtime whitelist or floor base
inference. All loaded high and low templates are separately decoded in every
scanned graphics context, including handlers not dispatched in that state.

Inventory: **387 dispatch assemblies (313 high,74 low),12,445 proved placements**.
21,575 expected BG footprint cells were not final-writer/equal-write provable;
these include BGs not written by a high dispatch, not just destructive
overwrites. No boundary-wrap or final-writer-word mismatches observed.
**24 high identities / six four-role sets**, all 24 also observed in proved
placements; no loaded-only high identities, conflicting high bands, high/low
identity collisions or explicit floor/door/stair vetoes among accepted assets.
**18 distinct low-template identities** explicitly excluded. Of **70 canonical
southeast wall annotations**,46 have no proved high-band use; their IDs are
listed in the JSON and remain unauthored, not treated as missing members of
these six sets. They require other-family/state provenance.

## Authoring results (IDs omit `v1:4bpp:`)

| Set | cap | upper | middle | foot | Result |
|---|---|---|---|---|---|
| reference | a4aa6ebd24fefeb7 | 503ab068dfa941d3 | 3bc2b767d36c52c3 | 31aba23efb31666b | unchanged |
| A | 763d8c266d053224 | 5d3bc6907dece26c | 21a81de1db2e6a51 | ea5078ff7bc62f34 | four added |
| B | 14c17494b614916c | 3d3ebf8894607de1 | 39b78d16de3f7acb | e75cf1bf1f0c6b12 | four added |
| C | 47b53287b9de4dad | 114a7c432323276e | 43ecfb30d33274db | 4dc1cd16e8b5967e | four added |
| D | e45e17af66ecc627 | 2981d7a1438cb7f9 | ed71fcb09d321d30 | e48afc9dc194123c | four added |
| E | 0221286379a51537 | 4c00243dfbb980a5 | 8b839534fd40a9d3 | f7e9ae78507ae068 | four added |

**20 assets added** to `remaster/profiles/ALttP/alttp-profile.toml`: five caps,five uppers,
five middles,five feet. They cover **6,793 observed high-band uses** (cap2025,
upper1884,middle1743,foot1141; includes repeated entrance/default contexts).
Five newly complete intrinsic sets plus the unchanged reference set. No claim
that their contextual placements have solved room heights. All twenty have
zero unproved other uses in this corpus after the priority-only recovery below.

### Previously rejected cap: dispatch proof, not a different geometric use

Saved `room-097-entrance--1.raw`: actual borrowed entrance39 from room098,
room151 (`0x097`), BG2 (layer index1), cell `(24,4)`, word**0x6880**, H flip,
no V flip, plane0, collision mode2, no in-room stairs in traversal state.
Final writer**39**, phase1 ordinal8, source offset26, raw**0x41fd**; ROM object
source address**0x1fb27d**, subtype2 object**0x39**, origin `(20,4)`.
`LoadType1ObjectSubtype2` case39 (Door Staircase Down L) writes 4x3 art using
template0x1160 at x20..23, then performs **`dung_bg2[dsto+4] |= 0x2000`**.
The questioned cell is outside the stair-art footprint. It is not stair art
and no image resemblance or nearby-floor ownership inference is needed.

Earlier writer**33**, high-diagonal handler**0x0f**, template**0x45c**,
origin `(24,4)`, six(+1,+1) columns, supplies cap word**0x4880**. The final
word is exactly this source word OR priority0x2000. The same column's upper,
middle,foot `(24,5..7)` retain exact words**0x4881,0x489a,0x489b**, all owned
by writer33. Thus the existing dispatch operation and surviving assembly
prove the **same canonical cap band** (not an alternative stair/door usage).

General discovery rule handles subtype2 staircase handlers38/39 priority-only
flanks at `dsto-1` and `dsto+4`. Require a later final writer, exact flank
coordinate, exact source word OR0x2000, one earlier high-diagonal footprint,
and all three other geometric bands still owned by it. Reject replaced art,
interior opening tiles, different handlers, missing/overwritten companions and
ambiguous ownership. The rule recovers exactly**one cell** over the entire
347-context scan. Cap count becomes**268 proved uses**, no unproved uses;
all twenty authorable assets have no band/low-topology/explicit-role conflicts.
The cap was then authored with exact reference arrays; no room/hash exception.
Before investigation details remain in `diagonal-sets-final/unproved-identity.json`;
the new `diagonal-sets-priority/unproved-identity.json` is an empty list.

Exact reference arrays copied, not reconstructed from shading. Heights remain
cap17..24,upper9..23,middle1..15,foot0..7: the whole assembly is zero-based,
not each tile's minimum. Reference cap rounding/detail normals and foot's mixed
wall/floor normals survive exactly. **No reference has an explicit occlusion
array**. Targets preserve the same absent/default semantics (runtime default
255 per pixel); transparency is not converted into a physical hole. Explicit
all255 arrays were tested but removed because native serialization intentionally
omits default coverage. Painted target masks are a rejection condition.
Nearest sampling matches the references.

All twenty already have canonical `wall/south_east/context_dependent=false`
annotations in `alttp-tile-semantics-v1.json`; no semantic source modification
is warranted. No automatic metadata activation/regeneration performed.
Profile groups/settings/materials/rules/masks, every old asset including all
four references, immutable captured profiles and backups remain **byte-exact**
against the pinned approved baseline after removing the twenty additions.

## Runtime and validation

This is actual intrinsic height/normal metadata authoring, not an overlay count
repair. Reloading the profile makes these arrays available to rendering and
the existing solver. No runtime/app code changed; **no rebuild needed**.
It does not activate absent semantic groups, solve unknown placement bases or
change the ROM-owned preview's existing cardinal-derived foot datum24. Effective
room placement remains contextual; overlapping gameplay planes are not merged.

Mandatory complete-room v2 rerun with the exact existing native runtime solver,
complete linked maps, collision planes, pinned states, complete doorway graphs,
new profile and compiled ALTPHM3 maps:

| Room | valid support | planes | solved/unknown BG2 walls | conflicts | exit |
|---|---|---|---|---|---|
|055|439/439|439,0|1391/1|0|1|
|060|711/711|437,274|399/62|1|1|
|061|1556/1570|1570,0|826/8|0|1|

These fixture results match preexisting blockers. **No room approved or
returned as gate-passing capture work.** Other316 scanned rooms have no
complete accepted doorway/support graph validation in this task. Room060's
upper/lower overlap is independently retained by existing plane arrays;
asset-local transfer does not supply new overlap proof. No fresh matching
Snes9x gameplay/camera comparison or visual approval performed.

Seven discovery/authoring regression tests pass (exact applied arrays, H/V normal/pixel
transforms, reverse bands, both BGs, overwrite rejection, low/topology and
explicit-role vetoes, context ambiguity, default-vs-painted occlusion, surgical
patch preservation, priority-only flank acceptance and negative replacement/
interior/wrong-handler/overwritten-companion/mixed-owner cases). Four diagonal
preview tests pass (two existing plus two compatibility/strict-input tests).
Native strict profile load/serialize/parse/canonical equality passes:
schema21,226 assets,225 height layers,203 normal layers,98 occlusion layers.
`git diff --check` passes. **All23 validator tests pass**, including the six
previous errors introduced by our earlier preview helper. Fixed only that
offline helper: legacy cell-only synthetic snapshots have neither graphics
layers nor a decoder and receive no diagonal proof (empty map); they still
undergo all ordinary validator checks. Real RoomSnapshot inputs missing these
fields, partially implemented decoding adapters, invalid layer shape and
truncated ROM template data explicitly fail. No guessed graphics base, fake
hash, default geometry or incomplete real capture bypass was introduced.

## Artifacts, reproduction and remaining capability

Final full machine evidence and all raw/state/provenance/attribute inputs:
`/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/diagonal-sets-priority/`:
`discovery.json`, `summary.json`, `authoring.patch`, `room-*-entrance-*.raw*`.
JSON enumerates exact bands/IDs, contexts/entrances/themes, flips, writer,
coordinates, omissions, pins, rejected reasons and all inventory denominators.
Generated artifacts are review evidence, not authoritative source.
Complete v2 JSON/SVG, compiled maps and pinned profile:
`.../diagonal-sets-priority/fixtures/` (rerun after final cap/helper change).
Earlier `diagonal-sets/` and `diagonal-sets-final/` evidence is preserved,
superseded by the priority discovery and final twenty-asset authoring.
An additional full corpus dry-run with the final edited profile is saved in
`diagonal-sets-priority/current/`: **zero remaining eligible updates**, all
twenty assets already equivalent, same319 rooms/346 completed contexts.

From repo root, with an existing new output directory:

```sh
python3 -B remaster/tools/ALttP/alttp_diagonal_sets.py \
 --renderer build/remaster-geometry/room-preview-engine-v21/alttp-room-preview \
 --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc \
 --atlas build/remaster-geometry/alttp-atlas.sqlite \
 --profile remaster/profiles/ALttP/alttp-profile.toml \
 --semantics remaster/tools/ALttP/alttp-tile-semantics-v1.json --output "$OUT"
```

Default is dry-run; `--apply` explicitly emits an `apply_patch` patch for
review/application, **never directly rewrites the profile**. Current profile
should yield zero remaining eligible updates. Final before/after evidence used
the immutable approved `diagonal-zero026/replay-input-profile.toml` as the
baseline. Verify exact application with:

```sh
python3 -B remaster/tools/ALttP/alttp_diagonal_sets_verify.py \
 --report "$ROOT/diagonal-sets-priority/discovery.json" \
 --profile remaster/profiles/ALttP/alttp-profile.toml \
 --baseline "$ROOT/diagonal-zero026/replay-input-profile.toml"
python3 -B -m unittest discover -s remaster/tools/ALttP -p test_diagonal_sets.py
python3 -B -m unittest discover -s remaster/tools/ALttP -p test_diagonal_preview.py
python3 -B -m unittest discover -s remaster/tools/ALttP -p test_alttp_validate_room_heights.py
python3 -B remaster/tools/ALttP/alttp_diagonal_unproved.py \
 --corpus "$ROOT/diagonal-sets-priority" \
 --atlas build/remaster-geometry/alttp-atlas.sqlite \
 --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc \
 --identity v1:4bpp:e45e17af66ecc627
python3 -B remaster/tools/ALttP/alttp_run_runtime_height_fixtures.py \
 --solver "$ROOT/diagonal-zero-solver" \
 --fixtures build/remaster-geometry/surface-audit-room-local \
 --profile remaster/profiles/ALttP/alttp-profile.toml \
 --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc \
 --output "$ROOT/diagonal-sets-priority/fixtures"
```

`ROOT` is the preapproved temp root above. No staging/commits/reset/revert or
exclusion changes. Owned source changes: profile plus this report,
`alttp_diagonal_sets.py`, `alttp_diagonal_sets_verify.py`, `test_diagonal_sets.py`,
`alttp_diagonal_unproved.py`, `alttp_diagonal_preview.py`, `test_diagonal_preview.py`.

**Enough information for all sets? No.** Enough to identify and author the
twenty equivalent intrinsic assets within this pinned corpus: yes. Missing
game-wide capability: real room-transition graphics-context enumeration rather
than nearest borrowed entrance, progression/switch/water/door state enumeration,
general equal-word write/overwrite history for broader unknown ownership, other
diagonal/overworld handler templates, accepted movement/support bases, and
matching Snes9x parity. Smallest next action is matching live inspection of the
newly authored sets, while pursuing real transition contexts/state enumeration
to close the ALL-sets denominator. Reload the edited profile for
live inspection; immutable frame packets retain captured metadata.
