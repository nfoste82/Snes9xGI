# All canonical southeast wall diagonals — reviewable but incomplete

## Latest autonomous continuation:64/70, six window gaps remain

Supersedes the58/70 and12-gap historical inventory below. Added six more
intrinsic assets without changing any previously authored array:

- Terminal trim4: `5f7c8513ac5716cd`,`f7cfa555cbb6dd13`, plus exact palette
  counterparts `12d48f1bc53551dd`,`0aa802669c855b43`: height0/+Z for all64 pixels.
- LOW foot2: `7b2f220c1310dd0a`, exact palette counterpart `0c84da43aae8aeca`:
  approved foot0..7 and reference mixed wall/floor normals.

Terminal trim proof is intrinsic, not guessed floor filling: all1278 observed
uses are exact final-writer/word matches of canonical band4 in stepped ROM
columns. Vertical flips reverse row mapping. The reference continuous cross
section at b4 is `max(0,min(24,31-32-x-y))=0` for every pixel; both north/west
joins meet the reference foot's zero seam. Its complete pixel region is the
terminal floor-side continuation, not a fifth vertical band. Canonical artwork
has no transparent idx0, so no physical hole inferred. Adjacent decoded floor
profiles include zero sources; contact variations only affect contextual base,
which is NOT transferred or solved by this authoring. Exact default full255
occlusion retained. These four flat arrays are proved terminal geometry, not
placeholder arrays to satisfy coverage. Canonical wall/SE annotation remains
unchanged because it labels the assembly family, not every pixel's normal.

LOW foot proof now accepts **immutable intrinsic template position**, separately
from unobserved placement: all60 loaded template records identify one LOW foot
band with consistent H/V inversion, three exact authored cap/upper/middle
companions and independent LOW cardinal24-rise proof. Full corpus has no final
uses or conflicting ownership of this identity. The ROM source slot is BG
pack20,tile28,high; alternate low conversion differs only palette-half expansion.
No evidence of actual runtime floor base or gameplay placement is claimed.
Rules reject conflicting bands/families, missing companions, painted masks,
context/role vetoes and already authored fields. No room/hash runtime whitelist.

**Remaining six:** `4527c0415be5feae`,`abe52639a643975e`,`d7283c7d2135e845`,
`65f25342cf31afce`,`f3b8b9d8b8ba394e`,`d5cfe79bacf8bfd5`.
Further autonomous seam investigation confirms all immediate window/cardinal
neighbors in both observed rooms have **no authored height/normal arrays**.
For example room00d at x8,y36..38 has words1da7/1da8/1da4, adjacent cardinal
window a21ed34c16480860/392c4f94fa95a297/f5e89d2c0018b352, lower trim
e6a3148d0529ac57 and background21ca8eccec66a061—all height/normal absent.
Thus immutable five-row window topology and art are known, but no corresponding
cardinal cross-section, seam datum, region normal boundary or solved support
evidence constrains the rise. Multiple different rises can reproduce the same
ROM tilemap; ordinary24-rise transfer would be fabricated. Need author/review
the shared Agahnim cardinal/window assembly cross-section and named high/low
regions, then diagonal seams and exact conversion solve these six together.
This is genuinely missing geometry evidence, not an unexplored identity search.

### Corrected role/GI and room061 validator findings

Explicit `tile_role` and semantic flags are **not required to apply asset GI
height/normal arrays**: strict parser independently sets hasHeight/hasNormals;
`S9xRemasterDungeonPlacements` consumes hasHeight outside its role checks.
Role/group metadata governs support/structural classification, not whether GI
arrays are active. Removed misleading `semantic_activation_unproved` blocker
wording from machine summary; report absent flags as facts separately. No bulk
role activation needed or performed for user intrinsic request.

Room061 mixed-floor error was a real validator/runtime mismatch exposed by LOW
foot authoring. At(8,38),4952263cea5a838f runtime asserts foot datum0,15 grounded
pixels. Validator only recognized generated dungeon_floor/wall_tops hashes;
runtime also recognizes explicit tile_role=floor. General validator fix adds
explicit floor roles and respects explicit wall/door/obstacle vetoes over stale
groups. It still requires exact-zero geometry, collision00, solved base and
same-plane reachable membership—no guessed graph or default height. Regression
guards role-only floor acceptance and stale-group/unknown-zero rejection.

Mandatory final fixture v2 now emits room061 JSON/SVG, **exit1**,1556/1570 valid
support,14 missing local/support placements,834 solved/0 unknown walls,0conflicts;
not approved.055 exit1,439/439support,1391/1walls;060 exit1,711/711support,
399/62walls,1conflict,planes437/274. No validator gate weakened to zero exit.

Latest profile SHA256
`7666fd8532f427258d317b7051e9f4a795f07046a718c19e582db8bb0f584d30`.
265assets/265height/243normal layers.44 tests pass (3remaining-rule,4gap,
10discovery,4preview,23validator); native strict roundtrip, exact six-array
verification, byte-exact preservation and diff-check pass. No runtime source
edits; validator diagnostic/classification tooling only. Parent owns no new app
build requirement. New tools: `alttp_diagonal_remaining_rules.py`,
`alttp_diagonal_complete_remaining.py`,`alttp_diagonal_remaining_probe.py`,
`test_diagonal_remaining.py`; source profile+validator+gap inventory/report updated.

Evidence remains under`diagonal-low/`: `remaining-plan.json` retains all rules,
source pins/contexts/arrays,`remaining-before-profile.toml` pins preserved prior
profile,`semantic-diagonal-gaps.*` current exhaustive70,`semantic-fixtures/`
latest complete-room JSON/SVG/ALTPHM3. Historical counts below retained as
before/after evidence. Parent TODOs: trim/foot intrinsic6 DONE; validator
role-only-floor mismatch DONE; window6 OPEN with missing cardinal cross-section;
room061 fourteen missing placements OPEN; fresh gameplay comparison OPEN.

2026-10-08 mandatory independent check. **Every wall annotation containing
south_east/southeast is a diagonal; missing intrinsic data is a gap irrespective
of known handler, loaded state or observed placement.** No denominator filtering.

## Result and new fix

Semantic universe70. After HIGH20/LOW10:34 complete,36 missing BOTH height and
normal arrays. This follow-up proves and authors24 more intrinsic assets:
**58/70 now have complete arrays;12/70 remain explicit unresolved gaps.**
All70 use absent explicit occlusion = approved reference-compatible full255
runtime default. Zero painted/explicit occlusion arrays in this diagonal
universe. Absent occlusion is not counted as a gap or transparent physical hole.

The additional24 are **exact ROM-source palette-half conversions**, not
artwork-similarity matches. Same background pack, tile index and SNES source
address; all64 low indices are0..7 and high indices are exactly
`low | (low != 0 ? 8 : 0)`. See `decode_3bpp_tile` in
`alttp_atlas_graphics.py` / game's Do3To4Low/High. Pixel coordinates and zero
mask unchanged. The alternate conversion's existing canonical SE annotation
agrees, one proved high/low source band exists, all other corpus uses of that
source are proved, and no conflicting target usage/mask/context/role exists.
Thus local heights/normals/default occlusion transfer unchanged; NO placement
floor base transfers. All24 target conversions have zero observed placements
or loaded contexts in this default corpus: intrinsic authoring is real, but
there is no new runtime placement coverage claim for them.

## All24 newly updated IDs (v1:4bpp prefix omitted)

|Band|IDs|
|---|---|
|cap|24d329f88ea6187c,28831f6d405a1ac7,3cd4b9cae5fb6f4d,5748e11bc58e8927,b961c1ccf2116432|
|upper|0c88b3e9d593079e,44be7136c6dbb2c5,5223ca41b8fbf719,a9bc75abd8bfffa1,adafeea556ff03cb|
|middle|41b6707bc22d9253,94077636e088b921,c1c3d8678f95d3e3,cc0f8f3007ae36bb,e37fdbeabe5c5782,ff0989d16a06677b|
|foot|133f891e3d7fa137,190cd1f95324f94e,4945d2445a19ed68,51be6fd2b65d97e4,8ea1240ca56f1b86,8ea9071ada55596b,ce9c2c1ad7a04a0e,f0be9007da34c8e2|

Combined authoring: preserved4 references + HIGH20 + LOW10 + conversion24 =58.
Final profile SHA256:
`b6c60a0eab810f964197239ba043f7f7f9e94de17c7b615f7b997e015fdb78b0`.
This task series added54 arrays-bearing identities, not all70. Existing source
profile before this follow-up is preserved byte-exact after stripping only
the24 new blocks; verify command below. All previously approved arrays, masks,
settings, semantic annotations, HIGH20/LOW10 and concurrent work survive.

## All12 remaining gaps and exact evidence needed

|ID|Known family/usage|Observed corpus|Missing proof/capability|
|---|---|---|---|
|0aa802669c855b43|BG pack18 tile43 LOW conversion of f7cfa555cbb6dd13|unloaded/unplaced|solve distinct diagonal trim regions/seams on source, then exact conversion|
|12d48f1bc53551dd|BG pack19 tile43 LOW conversion of5f7c8513ac5716cd|unloaded/unplaced|same trim-region proof|
|5f7c8513ac5716cd|HIGH/LOW diagonal template **trim** slot, not one of four wall bands|16BG2 cells,room109 hex,entrance76; final handlers0d/0e/0f/10|prove whether canonical regions are floor-side trim, continuing wall or transition; both cardinal contact heights and region normal boundary, no fifth-band guess|
|f7cfa555cbb6dd13|HIGH/LOW diagonal template **trim** slot|1262 cells,40 rooms listed in full inventory; handlers0d..10/19..1c|same mixed trim/wall region geometry; artwork is not the foot's triangular mask|
|7b2f220c1310dd0a|LOW foot template, BG pack20 tile28 HIGH conversion|10loaded layer-context records,0final placements|actual final LOW dispatch in matching graphics/state, surviving companions + contacts; loaded words alone not final ownership|
|0c84da43aae8aeca|LOW conversion of7b2f220c1310dd0a|unloaded/unplaced|prove above foot then conversion|
|abe52639a643975e|Agahnim window diagonal, BG pack24 tile39 LOW conversion|28BG2 cells,rooms00d/020|window assembly high/low region and cardinal seam datum; not equivalent generic HIGH band|
|4527c0415be5feae|same window diagonal,pack24 tile40 LOW|24BG2 cells,rooms00d/020|same window constraints and opposite-facing/region boundary checks|
|d7283c7d2135e845|same window diagonal,pack24 tile36 LOW|20BG2 cells,rooms00d/020|same window constraints; repeated slot phase is not local-height band evidence|
|f3b8b9d8b8ba394e|HIGH conversion of abe52639a643975e|unloaded/unplaced|source window geometry proof then conversion|
|65f25342cf31afce|HIGH conversion of4527c0415be5feae|unloaded/unplaced|source window geometry proof then conversion|
|d5cfe79bacf8bfd5|HIGH conversion ofd7283c7d2135e845|unloaded/unplaced|source window geometry proof then conversion|

Window final dispatch atlas subtype1 object2e is
`LoadType1ObjectSubtype2`→`RoomDraw_AgahnimsWindows`, NOT subtype1-loader floor
stair case2e. Handler SNES81:9ea3 first writes four-row window panes from1bf2,
then five stepped diagonal rows from1c22 at x2..8 and mirrored x23..29.
Their later priority/assembly geometry differs from ordinary diagonal walls.
Machine inventory retains final dispatch, coordinates, words, flips, source
slots and canonical decoded pixels for every occurrence. Do not apply the
24-unit ordinary wall pattern merely because canonical metadata says SE wall.
These ARE required diagonal gaps, not “out of scope.”

Additional LOW loaded-only foot `bc58650cd64647b1` is not in the70 SE annotation
universe, but remains a separately recorded general diagonal discovery blocker.

## Complete exhaustive70-ID inventory (C=arrays available,G=gap)

All entries have default full255 occlusion. C means intrinsic availability,
not room-height approval. Explicit profile role is absent except4952263cea5a838f
(`wall`). Family/rooms/conversion source details for EVERY entry are in the
machine report and generated70-row Markdown inventory described below.

```text
C 0221286379a51537  C 03e6150673f678eb  C 042b1f6fe3b7a372
G 0aa802669c855b43  G 0c84da43aae8aeca  C 0c88b3e9d593079e
C 114a7c432323276e  G 12d48f1bc53551dd  C 133f891e3d7fa137
C 14c17494b614916c  C 190cd1f95324f94e  C 21a32992dd5a1432
C 21a81de1db2e6a51  C 24d329f88ea6187c  C 28831f6d405a1ac7
C 2981d7a1438cb7f9  C 31aba23efb31666b  C 32a08f2b82a3bbe6
C 39b78d16de3f7acb  C 3bc2b767d36c52c3  C 3cd4b9cae5fb6f4d
C 3d3ebf8894607de1  C 41ad2b5f209bec6a  C 41b6707bc22d9253
C 43ecfb30d33274db  C 44be7136c6dbb2c5  G 4527c0415be5feae
C 47b53287b9de4dad  C 4945d2445a19ed68  C 4952263cea5a838f
C 4c00243dfbb980a5  C 4dc1cd16e8b5967e  C 503ab068dfa941d3
C 50d27476d4f95ff0  C 51be6fd2b65d97e4  C 5223ca41b8fbf719
C 5748e11bc58e8927  C 5d3bc6907dece26c  G 5f7c8513ac5716cd
G 65f25342cf31afce  C 763d8c266d053224  G 7b2f220c1310dd0a
C 8b839534fd40a9d3  C 8ea1240ca56f1b86  C 8ea9071ada55596b
C 94077636e088b921  C a07774b037c235c6  C a4aa6ebd24fefeb7
C a9bc75abd8bfffa1  G abe52639a643975e  C adafeea556ff03cb
C b961c1ccf2116432  C c1c3d8678f95d3e3  C c6c8bc5f9a10e349
C cc0f8f3007ae36bb  C ce9c2c1ad7a04a0e  G d5cfe79bacf8bfd5
G d7283c7d2135e845  C e37fdbeabe5c5782  C e45e17af66ecc627
C e48afc9dc194123c  C e75cf1bf1f0c6b12  C ea5078ff7bc62f34
C ecec2e21065071c6  C ed71fcb09d321d30  C f0be9007da34c8e2
G f3b8b9d8b8ba394e  G f7cfa555cbb6dd13  C f7e9ae78507ae068
C ff0989d16a06677b
```

## Role activation is NOT array coverage

Canonical sidecar:70 approved wall/SE annotations. Profile explicit wall role:
1/70;69 absent. Authored semantic activation flag:0/70. This audit does not
pretend that absent flags prove the runtime ignores a tile: ROM dispatch can
classify placements independently. It reports activation as unproved separately
from58 available intrinsic arrays and12 gaps. No bulk semantic regeneration or
role changes performed because that could change solving; correct contextual
ownership remains mandatory.

## Corpus, tests, gates and handoff TODOs

Same complete digest-verified corpus:320 attempted IDs,319completed,347 contexts,
346completed,213borrowed,2,834,432 placed cells. All70 queried against all maps,
all1024 decoded graphics slots on both BGs in every completed context, ROM source
slots/conversions and overworld base usage. Unloaded/unplaced entries remain in
the denominator. No state/graphics-context enumeration beyond this corpus.

41 tests pass (4semantic gap,10discovery,4preview,23validator), strict native
roundtrip passes, schema21/259assets/259heights/237normals/98occlusion layers.
Byte-exact previous-profile preservation and all24 applied arrays verified.
`git diff --check` passes.
Mandatory complete-room v2 rerun with final profile/runtime/ALTPHM3 fixtures:
055 exit1 (439/439support,1391/1walls);060 exit1 (711/711support,399/62walls,
1conflict,planes437/274);061 exit2 mixed-floor grounded-connectivity rejection.
No gameplay approval, no runtime source changed, parent app build unnecessary.

Files: profile, `alttp_diagonal_gaps.py`, `test_diagonal_gaps.py`, discovery's
independent semantic inventory integration, this report and prior report links.
Artifacts in preapproved temp root `diagonal-low/`: `semantic-diagonal-gaps.json`
and `.md` enumerate all70 with arrays/default occlusion, activation, families,
rooms, every placement, loaded contexts, decoded pixels, ROM sources and
conversion proofs. `semantic-before/` preserves36-gap baseline and24-update
plan; `semantic-fixtures/` preserves final-profile v2 results.

Reproduce (OUT existing, CORPUS original verified raw corpus):

```sh
python3 -B remaster/tools/ALttP/alttp_diagonal_gaps.py \
 --profile remaster/profiles/ALttP/alttp-profile.toml \
 --semantics remaster/tools/ALttP/alttp-tile-semantics-v1.json \
 --discovery "$ROOT/diagonal-low/current/discovery.json" \
 --corpus "$ROOT/diagonal-sets-priority" \
 --atlas build/remaster-geometry/alttp-atlas.sqlite --output "$OUT"
python3 -B -m unittest discover -s remaster/tools/ALttP -p test_diagonal_gaps.py
```

Parent TODO update: semantic-universe independent audit DONE; palette conversion
failure-class repair DONE24; unresolved geometry12 OPEN (trim4,window6,
loaded-foot2); role activation proof OPEN separately; room061 mixed-floor
validator blocker OPEN; fresh southwest/southeast gameplay inspection OPEN.
Reload edited repository profile for live **inspection only**, not approval;
immutable captures remain unchanged. Do not declare all diagonal sets complete.
