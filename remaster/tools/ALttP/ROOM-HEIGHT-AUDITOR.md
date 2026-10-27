# Tool-first dungeon room-height auditor

The reusable OpenCode agent is `.opencode/agents/room-height-auditor.md`, pinned
to `litellm/gpt-6.1-sol`.

The companion repair expert is `.opencode/agents/room-tile-repair-expert.md`
(renamed from `room-height-expert`). Both agent
definitions live in `.opencode/agents/`; the legacy `.opencode/agent/` location
is no longer used.

Invoke the auditor through a parent agent:

> Use room-height-auditor to audit rooms 0x001 and 0x061. Detection only;
> report all issue classes and unsupported coverage.

It also supports primary selection (`mode: all`). OpenCode sessions store their
selected model separately: when using it as primary, select gpt-6.1-sol too.
A new subagent uses the configured model. Project discovery may require a new
session after creating the definition. This project's `.opencode/` is locally
git-ignored; keep/copy that definition deliberately when sharing the setup.

## Mission and boundary

Run independent tools; interpret their reports. Do not perform a manual LLM
tile-by-tile audit. Tooling should enumerate the complete room, evaluate actual
effective sources, group failures, and emit evidence. Use targeted inspection
only for an unexplained machine-reported class or a user-reported false negative.
Turn discoveries into reusable scripts and regressions, not expanded prompts,
per-room whitelists, more visual scanning, or more agents.

Read `AIDocs/alttp/07-generalized-height-coverage.md` and the relevant surface
contract before auditing. You own discovery and detection-tool improvements;
`room-tile-repair-expert` owns authorized production repairs to concrete tile
issues, including issues beyond heights. Do not outsource the audit to manual
whole-room analysis or automatically launch repairs. See
`ROOM-TILE-REPAIR-EXPERT.md` for the distinction and handoff. Runtime/profile fixes
are outside a detection request. Tool/test improvements are in scope; preserve
all existing authored data and unrelated work. Do not auto-commit changes.

## Run protocol

1. **Scope:** normalize requested hexadecimal IDs; declare entrance/state,
   graphics context, planes and available captures separately for each room.
   An unspecified state is not authorization to claim all states inspected.
2. **Preserve:** record `git status --short`; create a unique output directory
   under `build/room-height-audits/`. Copy selected profile bytes, record SHA256
   of every input, source/binary, ROM and state, and save exact commands/logs.
   Never trust an old ignored binary. Compile current diagnostic sources.
3. **Discover evidence:** locate complete fixtures and capture sidecars; check
   their room IDs, state, profile and identities, not just filenames. Reproduce
   capture own-input geometry before comparing a research/current profile.
4. **Run the applicable tools below.** Maintain a matrix per room/rule:
   executed, failed, passed-conditionally, unsupported, evidence-missing, or
   not-applicable-with-evidence. An unavailable input is not a skipped pass.
5. **Inspect structured summaries first.** Programmatically group by rule,
   writer/assembly, layer/plane and source stage. Retrieve a few representative
   pixel/placement records per group. Do not dump entire ledgers into context.
6. **Check coverage:** 8192 BG placements and 16384 layer/plane records where
   required; transparent, non-geometric, hidden, unobserved and unsupported
   records must remain separately accounted. Zero reachable support is empty
   evidence, not successful coverage. Do not merge disconnected datums.
7. **Handle misses with tooling:** use the bounded improvement loop below.
8. **Report every failure class**, including old failures. Preserve report JSON,
   command manifest, pins, stderr, diagnostic crops and explicit evidence gaps.

For multiple rooms, reuse binaries only while source digests match; isolate
each room/state's outputs and do not let an expected detector exit1 abort the
rest of a batch. Independent runs can execute in parallel with bounded resource
use. Do not spawn one manual inspection agent per room.

## Evidence discovery

Canonical current profile: `remaster/profiles/ALttP/alttp-profile.toml`.
Historical reproducible fixtures: `build/remaster-geometry/surface-audit-room-local/`.
Atlas: `build/remaster-geometry/alttp-atlas.sqlite`.
Local capture directory: `~/Library/Application Support/Snes9x/Remaster/`.
Use glob/read/grep for discovery and inspect tool `--help` before invocation.
Do not search unrelated personal directories or upload ROMs/captures.

A fixture usually has `room-XYZ-door-input.raw`, sibling `.attr`,
`.state.json`, `.prov`, plus `room-XYZ-doorway-graph.json`. A live capture
usually has `.s9xrmf`, `.dungeon.json`, `.profile.toml`, `.dungeon-input.bin`,
`.dungeon-templates.bin`, and possibly `.terrain-input.bin` appended to its
filename. Presence alone does not establish validity.

Supported unheadered USA ROM: 1048576 bytes, SHA256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Discover a local verified copy; never substitute a ZIP or change the ROM.

Reference renderer maps can establish provenance only after complete BG-word
equality; also compare collision arrays and decoded placed identities. Equal
maps do not prove equal graphics animation, profile, movement state or entrance.
Fresh live traversal must match the seed/state; do not borrow stair routes by
editing state JSON. Old renderer versions may ignore route seed options.

## Build current diagnostic consumers

Run from repository root. `$OUT` is the unique existing task directory.

```sh
c++ -std=c++17 -O2 remaster/tools/ALttP/alttp_runtime_room_heights.cpp -o "$OUT/runtime-solver"
c++ -std=c++17 -O2 remaster/tools/ALttP/alttp_terminal_height_inventory.cpp -o "$OUT/terminal-inventory"
c++ -std=c++17 -O2 remaster/tools/ALttP/alttp_capture_height_pixels.cpp -o "$OUT/capture-pixels"
c++ -std=c++17 -O2 remaster/tools/ALttP/alttp_capture_debug_sources.cpp -o "$OUT/debug-sources"
```

These are header-based portable consumers; do not separately link `sha256.cpp`
when the consumer already includes it. Direct solver/terminal arguments are:

```text
SNAPSHOT ATTR PROFILE linkX linkY plane [stairX stairY highNorth changesPlane]... --rom ROM
```

Obtain seed and stairs from the pinned state's schema, not defaults. Save
terminal stdout to JSONL and solver stdout to JSON. The terminal inventory must
start from **immutable profile metadata**, apply production transport, and use
`S9xRemasterResolveFrameGeometry`. Classifier `local`/`t.height` is not actual
runtime geometry. Check `has_effective_height`, asset/placement/context source,
geometry status, selected base and `actual`; never treat zero-filled missing
arrays as authored geometry. An existing detector not enforcing a source flag
does not relieve the orchestrator of reporting that coverage gap.

## Tool matrix and commands

`$PROFILE` below means the explicitly pinned profile for this run, not a mutable
path. `$RAW` means the matching reference-schema snapshot with sibling `.prov`.
All scripts live under `remaster/tools/ALttP/`; use their full paths.

### Runtime reproduction and validation v2

* `alttp_replay_live_height_evidence.py "$CAPTURE" --solver "$OUT/runtime-solver"
  --rom "$ROM" --output "$OUT/replay"`: checks complete live input, own-profile
  FNV, publishability, ROM templates and WRAM/PPU equality. Its generated raw
  wrapper is solver input, **not rendered gameplay evidence**. Research-profile
  overrides must be labeled separately.
* `alttp_run_runtime_height_fixtures.py --solver ... --fixtures ... --profile ...
  --rom ... --output ...`: hardcoded 055/060/061 runner, not arbitrary room
  selection. Its map export has representation limitations; do not waive errors.
* `alttp_collision_reachability.py --attributes "$ATTR" --room "$ROOM"
  --state "$STATE" --stair-trace "$TRACE" --output "$OUT/graph.json"`:
  matching initialized collision and observed traversal. State and stair trace
  go together; additional directed evidence uses `--extra-stair-trace`.

Mandatory complete-room check (omit a map only when none exists, and say so):

```sh
python3 -B remaster/tools/ALttP/alttp_validate_room_heights.py \
  --graph "$GRAPH" --snapshot "$SNAPSHOT" --attributes "$ATTR" \
  --state "$STATE" --profile "$PROFILE" --runtime-solution "$SOLUTION" \
  --height-map "$MAP" --rom "$ROM" \
  --provenance-snapshot "$RAW" --atlas "$ATLAS" \
  --output "$OUT/validation-v2.json" --diagnostic-svg "$OUT/validation-v2.svg"
```

Require matching complete planes/state and explicit denominators, local sources,
floor bases, fallback directions, missing/conflicts. Read configured rise from
the profile and pass `--rise` consistently if needed; configured rise50 and an
authored stair relationship24 are different facts. Exit0 is a minimum gate, not
complete geometry proof. Exit1 reports failed checks; exit2 is invalid input.
Do not generate approval from a screenshot or array-presence count.

### Doorways; diagonal and straight-stair structure contracts

Run **both** `alttp_doorway_detection.py` and `alttp_structure_detection.py` with:

```text
--snapshot RAW --attributes ATTR --state REFERENCE_STATE --profile PROFILE
--rom ROM --consumer TERMINAL_BINARY --output REPORT.json
```

The structure detector additionally needs `--atlas ATLAS`. They freshly invoke
the terminal consumer. Reference state uses `room_id` and stair dictionaries;
live `.dungeon.json` uses a different schema. Do not interchange them.

Door contracts: explicit support ownership, BG1-derived split-BG geometry, open
interior floor continuity, Door-role structural adjustment, per-pixel relief and
transforms. Structure contracts: proved diagonal shapes/pixel masks, derived
local-source transport, and intact inter-room stair datums. Expectations may be
conditional on external floor observations; say so. Unsupported dispatches and
partial overwrites are not passes. Both may exit1 for expected findings/gaps.

An intact assembly containing explicit `Door` panels additionally emits
`closed_assembly_authored_relief` records when its complete authored member arrays
are single-band (0..7). This **separate conditional closed-barrier contract** does
not treat Doorway-only masks or authored zero samples as open-state cutouts.
Keep ordinary/open-frame and closed-barrier records separate when counting unique
placements. Dynamic writer history is not established by a Door role or equal map
words; missing/contextual/multiband authoring stays unsupported. For a smaller
datum-independent source/relief diagnostic, run `alttp_authored_door_detection.py
--profile PROFILE --terminal TERMINAL_JSONL --output REPORT.json`; constant
authored-to-actual residual is not proof of a correct absolute placement datum.

### Actual capture geometry and source admission

```sh
"$OUT/capture-pixels" "$CAPTURE" "$OUT/capture" > "$OUT/pixels.jsonl"
"$OUT/debug-sources" "$CAPTURE" > "$OUT/debug-sources.jsonl"
python3 -B remaster/tools/ALttP/alttp_capture_structure_join.py \
  --capture "$CAPTURE" --pixels "$OUT/pixels.jsonl" --snapshot "$RAW" \
  --contracts "$CONTRACTS" --all-contracts --output "$OUT/capture-join.json"
python3 -B remaster/tools/ALttP/alttp_exposed_height_detection.py \
  --snapshot "$RAW" --rom "$ROM" --atlas "$ATLAS" \
  --terminal "$OUT/terminal.jsonl" --pixels "$OUT/debug-sources.jsonl" \
  --state "$CAPTURE.dungeon.json" --output "$OUT/exposed.json"
```

Run contract joins separately for doorway/structure reports. Without
`--all-contracts`, the join defaults to diagonals only. It verifies complete map
identity and observed hash/flip/canonical pixel/camera. Source diagnostic reads
the exact CPU field producer: real main/sub/scene, opacity, effective heights,
normals, role and field validity. Purple is invalid field data, not necessarily
a missing height key. Distinguish valid BG1 rejected by scene admission from a
BG2 ownership error; transparent BG2 exposing BG1 is often intentional.

Exposed-height detection checks ROM/writer-backed cross-BG cardinal joins and
scene receivers. It requires actual capture exposure: map-only candidates stay
unobserved. **Normal exit0 may still contain failing seams/scene issues**: parse
the JSON. Raw-main and lighting-scene heights can legitimately differ; deduplicate
repeated scene/sub physical pixels while retaining their source identities.

### Known regression fixtures and tests

`alttp_verify_room061_detection.py --audit-dir DIR --output REPORT` checks the
known Room061 detector outputs, exact arrays/mismatch indices and BG1 evidence.
This is a **test oracle**, not a production room-coordinate rule. Read its input
requirements and generate fresh shared detector outputs; do not reuse historic
JSON as a current regression pass. The historical oracle asserts defects and
must not be used as a post-repair green gate. For before/after acceptance use
`alttp_compare_height_contracts.py --before BASELINE_REPORT --after FRESH_REPORT
--terminal FRESH_TERMINAL_JSONL --snapshot PINNED_RAW --output COMPARISON_JSON`
separately for each detector family. It retains every baseline pass/fail member's
exact expected array, transforms canonical terminal samples using pinned map
flips, requires actual selected-source availability, and rejects changed/missing
expectations. Exit1 on baseline-as-after is a useful negative control, not a test
failure to suppress. It is a regression comparator, not new room discovery.
Also run relevant `test_alttp_*detection.py`,
`test_alttp_capture_structure_join.py`, `test_alttp_verify_room061_detection.py`
and `test_alttp_validate_room_heights.py` with unittest discovery. Source/scene
regressions are in `remaster/tests/dungeon_diagonal_source_detection_test.cpp`
and `remaster/tests/scene_source_detection_test.cpp`; inspect argument usage.
Run `git diff --check`. Tool changes require targeted tests; runtime changes,
if separately authorized, additionally require the curated full gate and v2.

## Tool limitations not to hide

Additional detection-only diagnostics (inspect `--help`):

* `alttp_layered_trace.h` is a read-only callback for isolated capture probes;
  trace before/after profile selection, scene admissions and final EndFrame.
  `alttp_layered_channel_detection.py --trace TRACE.jsonl --output REPORT` groups
  independent opaque BG1 subscreen donors against decoded BG2 ink and actual
  scene ownership, retaining channel absence separately. A missing normal must
  not conceal a present height; absent metadata is not transparent artwork.
  Legitimate opaque BG2 and different owners are not automatic borrowing cases.
  This is a source-conservation diagnostic, not an absolute datum/semantic
  admission oracle. Keep capture-own assertions before research overrides.

* `alttp_capture_source_activation.py --profile PROFILE --pixels DEBUG_JSONL
  --state LIVE_JSON --output REPORT`: inventories authored versus selected height
  and normal availability across opaque captured BG buffers. Missing authoring is
  separate from failed activation; buffer duplication retains ownership.
* `alttp_surviving_provenance.py --reference REF_RAW --live LIVE_RAW --output
  NEW_RAW`: explicit **research-only** ownership transfer. Uses live maps/graphics,
  invalidates every changed word (including priority changes), and records its
  transfer manifest. Equal words do not prove live/dynamic writer history. Never
  replace replay identity assertions or label this captured provenance.
* `alttp_door_extension_detection.py --snapshot RAW --terminal TERMINAL_JSONL
  --profile PROFILE --doorways DOOR_REPORT --output REPORT`: conditional same-writer
  full-width outer wall bands beyond the ordinary twelve-member door. Exact
  landing, single-band linear authored relief and uninterrupted ownership required.
* `alttp_cardinal_band_detection.py --snapshot RAW --rom ROM --atlas ATLAS
  --terminal TERMINAL_JSONL --output REPORT`: internal same-writer linear-band
  discontinuity **diagnostic**, not a general correctness oracle. Reused cardinal
  dispatches can legitimately meet stair/overlay ownership. Keep candidate failures
  conditional/unobserved until independent surface ownership and exposure exist;
  in particular do not contradict a user-approved room from map-only candidates.

* `alttp_scene_cardinal_continuity.py --snapshot RAW --rom ROM --atlas ATLAS
  --terminal TERMINAL_JSONL --profile PROFILE --pixels DEBUG_JSONL --state LIVE_JSON
  --output REPORT`: conditional same-writer observed cardinal-band edge continuity
  using the actual selected scene/main source and independent transformed authored
  slopes. It retains terminal bases for diagnosing selection rather than treating
  the standalone terminal as the captured scene. It is not a floor-anchored whole
  assembly oracle; nonunit artwork, hidden edges, cross-writer joins, corners and
  offscreen surfaces remain unsupported. Authenticate capture/PPU/map/scroll first.
  Reference-transferred writers are conditional, not captured dynamic history.

* No turnkey arbitrary-room/all-state proof exists. A blocked rule must appear
  in the report with the exact missing input or general tooling capability.
* `alttp_capture_complete_gate.py` is a convenience wrapper; its process exit0
  does **not** propagate nested v2 failure. Inspect `gate.json.exit` and scope.
* `alttp_room_height_issue_inventory.py` contains historical coordinate filters;
  do not present it as an exhaustive arbitrary-room detector.
* Surface-audit flat-art heuristics, nearest floors, legacy normalization,
  prototype geometry and role overlays are not correctness oracles.
* Historical reports/counts and profile hashes are evidence, not current truth.
  Room061's formerly passing diagonal checks bypassed missing source transport;
  their old pass counts are superseded. A user-authored fix requires fresh pins.

## Bounded improvement loop

On a miss or unknown, save a minimal pinned reproducer. Identify the stage:
input discovery/activation, provenance, classification, support/constraint,
runtime source transport, scene admission, raster visibility, or reporting.
Develop a general check using explicit ownership and independent contracts;
include flipped/overwritten/ambiguous negative cases and exact partial pixels.
Make the original case fail before the diagnostic improvement and be detected
after it. Keep detector repairs separate from geometry repairs. Rerun available
related fixtures and record actual corpus scope. If a full ledger/crop must be
reviewed repeatedly by an LLM, add structured extraction/grouping to the tool.
If an unknown cannot be resolved safely, stop that class with a precise blocker;
continue independent checks rather than manufacturing a clean room verdict.

## Required output

Start with outcome per room/state: defects found, incomplete/blocked, or current
checks passed with declared limits. Include:

1. Scope/pins and automated rule matrix, with evidence paths and commands.
2. Every issue group: room coordinates, BG/plane/buffer, source stage, actual
   versus expected pixel values, detector name, confidence and assumptions.
3. Counts: distinct placements/pixels vs repeated buffers, required/checked/
   failing/missing/conflicting/unsupported/unobserved/excluded denominators.
4. Evidence gaps and detector blind spots separately from confirmed defects.
5. Tests, tooling improvements and before/after detection counts if applicable.
6. Next smallest reusable tooling/evidence step; no unrequested geometry fix.

Do not say “all issues found” based on current checks. Do not omit previously
detected groups to emphasize new discoveries. Reports must be understandable
without the agent's conversation history. Detailed historical examples live in
`remaster/historic-investigations/doorway_detection_report.md`,
`remaster/historic-investigations/room061_detection_followup_report.md`,
`remaster/historic-investigations/room001_detection_investigation_report.md`, and
`remaster/historic-investigations/room001_source_seam_detection_report.md`;
read only relevant sections. See the [historical index](../../historic-investigations/00-index.md).
