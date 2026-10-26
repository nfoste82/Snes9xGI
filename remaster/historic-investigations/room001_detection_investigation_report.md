# Room061 regression; Room001 investigation — reviewable but incomplete

**Follow-up 2026-10-26:** see
[`room001_source_seam_detection_report.md`](room001_source_seam_detection_report.md).
Fresh captures006/007/008 prove the author's added throne heights are active;
remaining purple pixels fail scene admission (missing normals / omitted Obstacle
role), not current height-array availability. The follow-up independently proves
the exposed cross-BG wall seam mismatch without runtime/profile repairs.

2026-10-26. **Detection only. No runtime, profile, authored arrays or production
placement maps changed. Neither room is approved.** Concurrent profile and
obstacle work was preserved. All new builds/results are isolated under
`build/room061-001-detection-20261026/`.

## First: prove Room061 detection

Freshly compiled the shared `alttp_terminal_height_inventory.cpp`, then ran both
shared detector CLIs on complete pinned 055/060/061 fixtures. The consumer starts
from immutable profile metadata, retains flip/facing selectors, calls the real
application function and reads actual samples through
`S9xRemasterResolveFrameGeometry`. It does **not** preload classifier `t.height`
as the actual source. Added effective-source availability, placement/context
override, actual base and separate plane-base fields to its complete inventory.

New `alttp_verify_room061_detection.py` is a **fixture oracle only**, not a
production whitelist. It verifies every expected failing placement and all 64
expected/actual samples/mismatch indices, explicitly including unchanged-span
one-pixel errors. It reinvokes the shared consumer to retain correct BG1 versus
broken BG2 evidence. `confirmed061-regression.json` records exact evidence:

|Confirmed class, BG2 cell coordinates|Detected failures|
|---|---:|
|North interroom stair `(31..32,5)`, actual7..11 vs expected31..35|2|
|Upper-left interior `(3..4,16..17)`, actual0 vs24; frame `(3,15),(3,18)`, one pixel each|6|
|South upper band `(30..33,58)`, actual16..39 vs0..23|4|
|Lower-left `(5,31..34)`, actual0..7 vs16..23; `(6,31),(6,34)`, partial0..15 contract|6|
|East `(56,32..33),(57,31..34)`|6|
|SW surviving high diagonal cap/upper/middle, transformed V|27|
|SE surviving high diagonal cap/upper/middle, transformed HV|27|

The 54 high diagonal failures are missing transported derived local sources,
not permission to add a uniform24 offset. Fresh captured own-input regression
also passes: 54 complete-room omissions, 27 observed SE members /1728 main
pixels. Room061 structural contracts retain112 unresolved diagonal members,
44 conditional passes,24 exclusions; stairs6passes/2fails/8exclusions. Doorways
22fails/26passes/44 unsupported writer cells. No unknown is a geometry pass.

Corpus actually checked: three pinned default fixtures055/060/061,24576 BG
placements per detector. Doorways:05520pass/16fail/24unsupported;0606pass/
42fail/24unsupported. 055/060 have no supported diagonal/straight-stair
dispatches in this scope. No other317 rooms or alternate states claimed.
Fresh Room061 complete-room v2 exits1; see `room-061-validation-v2.json/.svg`.

## Then: Room001 complete investigation

### Pins and admissibility

USA ROM SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Current profile copied before investigation: SHA256
`48cab744f08a15e2b6e0c1ce30ca20ad903a11152ac23d14b5492f45656b99bc`,
serialized FNV `7b2f260489f8d515`, configured rise50. This is also live002's own
profile, unlike the historical profiles in live000/001.

Generated complete reference room through entrance123 (borrowed source room000,
theme19/main7/sub0), verified ROM. Its original seed383,137/plane0 cannot move:
one graph position, **zero body cells**. That empty graph is not room coverage.
The available live002 bundle instead records previous room002, settled7/0,
Link932,120 (local420,120), plane1, scroll768,12 (local256,12), collision mode0,
no stairs. No historical entrance is attested for live002.

Reference and live002 match **8192/8192 final words,8192/8192 collision bytes,
and8192/8192 placed decoded tile arrays**. Complete WRAM/PPU comparison also
passes. Reference palette/game state is NOT live parity: reference floor colors
and actors differ. Never use the gallery's colors as gameplay proof.

The old v21 binary silently ignored the documented diagnostic route seed.
Therefore built `fresh-room-preview` with the **current adapter source** and
existing ROM-verified reference object files, without overwriting the old engine.
It explicitly logs `diagnostic_route_seed ...420,120 plane1`. Its81-frame
W40,E40 route preserves complete maps/attributes and every ordinary position
occurs in the graph. This is a diagnostic relocation to a separately observed
seed, not an authentic entrance or an elevation anchor. Fresh six-camera views
cover both complete maps; priority/opacity/canonical-index identity checks have
zero mismatches. Lower-section actors/reference gameplay remain unverified.

### Actual findings, not just pass labels

Coordinates below are room-local8x8 cells; layer numbers are BG1/BG2, NOT
gameplay-plane numbers. Expectations remain conditional on exact external
floor contacts and surviving ROM dispatches, not absolute independent anchors.

1. **Both side doorway interiors have wrong effective BG1 heights. High
   confidence, automatically detected, and right side verified in live002.**
   BG1 `(6..7,16..17)` and `(56..57,16..17)`: actual16, expected0 at all64
   samples each. Exact same-layer landing contacts have datum0, local opening
   relief0. The surviving split door templates/writers65/66 establish ownership.
   This is the shared contextual doorway support/selection failure class, not
   missing asset arrays. The right interior is **BG1 correctly exposed through
   BG2 transparency**: scene and subscreen each show256 failing pixels. It
   would be incorrect to report missing BG2 floor geometry there.

2. **Four doorway-frame one-pixel defects. High conditional confidence,
   automatic.** BG1 `(6,15),(6,18),(57,15),(57,18)` each has one mismatch despite
   actual/expected spans both0..15. The right pair contributes two observed
   mismatches in scene and two in sub. Exact arrays and indices in
   `live-doorways.json` and `doorway-capture-join.json`; no flattening proposed.

3. **Don't mistake raw-main geometry for the lighting scene.** Shared terminal
   detector says the eight BG2 outer side-band members pass at16..23 in plane1;
   live002 right `(58,15..18)` main buffer retains0..7 while scene has16..23.
   Production swaps the scene into main during application, then restores it.
   This is a **buffer-scope distinction**, not evidence that scene geometry lost
   the height. Similarly north BG2 `(32,7)` is raw-main0..7 versus scene55..62.
   Own-profile fresh production replay reproduces the original packet exactly;
   there are zero effective-height/source differences over144930 BG samples.
   Join now supports **all contracts**, not just diagonals, while preserving
   buffer/layer identity. Its772 mismatches=256 raw-main outer-band samples +
   258 sub +258 scene doorway samples, NOT772 independent bad visible pixels.

4. **Two true absent effective local sources, not guessed flats.** BG1
   `(31,12)` hash`6469c78eea086416`, `(32,12)` hash`74b92900d763d251`, final
   writer50:64 opaque pixels each, Floor metadata but no authored/effective
   height source. They are hidden by opaque BG2 in the reference views and not
   observed in live002. This blocks complete geometry; it is NOT a visible BG2
   error. Expected local relief remains unresolved; zero was not authored.

5. **Complete-room residuals remain genuine blockers.** Plane1 live graph:
   15856 legal positions/368 body cells; plane0 zero activated positions.
   Exact-state v2:368/368 supports,6/6 fallbacks,444 solved wall claims,
   **480 unknown BG2 walls**,20 marked doorway placements/4accepted,
   zero runtime structural conflicts, no compiled map. Exit1. Unknown walls
   form the separate southern section, rows36..61; no arbitrary north-to-south
   elevation relation is inferred. V2 additionally reports directional wall
   continuity8 seam conflicts/3654 missing or unresolved edges over10464 edges.
   Example BG1 `(24,11)` south edge8 versus obstacle-support contact0 at
   `(24,12)`: diagnostic step-8 vs required0. Actual authored obstacle edge
   relief differs, so this is a **contact-contract review**, not yet a proved
   visible defect or permission to flatten the statue. Four statue assemblies,
   24 BG1 cells, have accepted support bases24/24 and16 witnessed contacts;
   that does not override the seam discrepancy. No independent stair or
   diagonal dispatch exists here: zero checked is not general shape approval.

### Whole-map denominators

Complete8192-cell inventory in `room001/complete-live-ledger.jsonl` retains
writer/word/flip/role, opaque/transparent disposition, immutable/classifier local,
terminal actual source/base/plane arrays, and captured main/sub/scene counts.
BG1:4096 opaque,2 absent local sources; BG2:3348 opaque,748 transparent,
zero absent terminal sources. Presence is NOT correctness. Unknown-role BG2
2084cells=748 transparent +1336 opaque out-of-bounds identity; these are explicit
unclassified/exclusion candidates, not1336 solved structural placements.
Live002 observed placements: BG1 main0/sub928/scene357; BG2 main545/sub0/scene554.
Complete map views cover all opaque BG2 placements but cannot observe hidden BG1.
Both planes retained independently: no overlap cells in the observed graph;
cross-plane relationship and unactivated plane0 remain unresolved.

Three available Room001 gameplay bundles000/001/002 replayed with their own
profile pins:144930/139402/144930 identity-verified BG samples, zero effective
height/source differences. These are two north camera positions (001 differs),
not three matching-current-profile/all-room gameplay states. No fresh Snes9x
capture was obtained, and no south-section gameplay comparison is claimed.

## Tests and next action

23 detector/oracle tests,4 capture-join tests,26 validator tests pass. Fresh
capture014 source regression passes. Existing transformed/ambiguous/overwritten
negative cases remain in the shared detector suites. Exact seven-class fixture
verification passes after rebuilding and rerunning both scripts. `git diff
--check` passes. Runtime unchanged: no application build required.

Reproduce Room061 oracle after running the shared detector CLIs:

```
python3 -B remaster/tools/ALttP/alttp_verify_room061_detection.py \
  --audit-dir build/room061-001-detection-20261026 \
  --output build/room061-001-detection-20261026/confirmed061-regression.json
```

Smallest next authorized repair: shared split-doorway open-interior support and
frame source-selection logic, then exact packet/whole-room regression. Separately
close intrinsic BG1 throne source metadata and acquire south-section movement /
live evidence before relating disconnected geometry. No room001 rectangles,
nearest-floor fills or runtime/profile repairs were made here.
