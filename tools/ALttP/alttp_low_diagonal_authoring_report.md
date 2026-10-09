# Room061 / settled capture029 — reviewable but incomplete

**Mandatory semantic-universe follow-up:** see
`alttp_diagonal_semantic_gap_report.md`. All70 canonical wall/SE identities are
required diagonals. Another24 exact ROM palette-conversion variants authored;
58/70 intrinsic complete,12explicit gaps. Counts/profile digest below pin the
earlier LOW10 pass, not the final follow-up profile. No earlier arrays changed.

2026-10-08. **Actual omitted LOW intrinsic family fixed. Not complete-room
approval.** No runtime source changed by this task; parent owns app builds.

## Symptom → cause → shared fix

Southwest diagonal assets were genuinely missing heights/normals, both in the
current authoring profile and immutable capture029 profile. Previous discovery
explicitly excluded all LOW identities. Metadata absence was the failing stage,
not cache/reload, artwork identity, camera mapping or diagnostics.

Supported ROM SHA256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Latest available room061 complete capture is
`~/Library/Application Support/Snes9x/Remaster/frame_room061_029.s9xrmf`:
settled submodule0, collision mode0, plane0, rise50, graphics base16384,
scroll[512,3261], Link[625,3368], profile serialized FNV`f74055715c8f06ed`.
Pinned profile digest verified. All8192 live WRAM words agree with PPU VRAM and
the complete provenance-enabled renderer room061 entrance4 export. This is
complete map parity, not a fresh after-authoring gameplay comparison.

ROM `LoadType1ObjectSubtype1` (atlas subtype0) handlers15..20 hex write five
rows to both BGs at each stepped column, minimum6 columns;15/18/19/1c/1d/20
step(+1,-1),16/17/1a/1b/1e/1f step(+1,+1). Templates498..506 hex.
Southwest handler**1a**, template**4ca**, has words:
`90ab,889c,884b,884a,8849`. V flip reverses canonical band order. Four wall
bands are followed by a **separate floor trim**, not a fifth wall height band.
Southeast companion uses H+V flips, the same canonical arrays.

LOW is not shorter in this reference convention: LOW cardinal N/W assemblies
independently span23..0 over three face bands, hence24-unit rise and foot datum0.
HIGH cardinal faces span47..24, hence same rise but different datum24. The
previous HIGH preview datum must NOT be copied to LOW. Discovery now requires
independently authored cardinal evidence (second N column at330 and first W row
at308 hex), both three-band ramps, before accepting LOW transfer. The first N
column reuses one identity in two bands and is NOT claimed as independent
unambiguous evidence; contextual cardinal reuse is a separate defect class.

With four geometric bands and independently constrained24-rise/zero foot, the
approved reference formula is applicable:
`height(b,x,y)=max(0,min(24,31-8*b-x-y))`, b=0..3.
Whole assembly is zero-based; upper/middle tiles are not individually zeroed.
Canonical SE semantic direction is already approved for all accepted identities.
Reference rounded cap/detail/foot regions and normals are copied exactly;
placement V transforms SE to NE at the southwest, H+V to NW at the southeast.
This is full-wall direction, not a lighting-facing exception or hash whitelist.
No explicit reference occlusion exists: retain absent/default255 coverage,
including transparent artwork pixels; do not manufacture physical holes.

## Updated intrinsic IDs

IDs below omit `v1:4bpp:`. All ten lacked arrays before this task; foot
4952263cea5a838f already had `tile_role="wall"`, preserved. Nine new asset
blocks plus two arrays added to that existing block.

| Band | Southwest ID | Other proved LOW IDs | local span |
|---|---|---|---|
|cap|50d27476d4f95ff0|042b1f6fe3b7a372|17..24|
|upper|32a08f2b82a3bbe6|03e6150673f678eb|9..23|
|middle|c6c8bc5f9a10e349|21a32992dd5a1432|1..15|
|foot|4952263cea5a838f|41ad2b5f209bec6a,a07774b037c235c6,ecec2e21065071c6|0..7|

Southwest before: all four height/normal arrays absent. After: exact64-height
and192-normal arrays above. Machine report includes **all44 proved southwest
placements**, both BGs (22 each), writer/handler/flips, complete before/after
arrays and displayed heights, not just a screenshot selection. Floor trim
`1a032ad7756c49db` remains its approved floor/zero metadata, unchanged.

## Corpus and preservation

Reaudited digest-verified original complete inputs: attempted320 IDs/347
entrance contexts;346 completed/319 IDs,213 borrowed contexts;2,834,432 placed
cells across both BGs. Room017 failed to settle.387 assemblies (313HIGH,74LOW),
12,445 proved placements,21,575 footprint cells lack final-writer/equal-write
proof (includes BGs not written by HIGH). No ownership guessing added.

10 proved LOW geometric IDs cover**2,704 uses**: cap868,upper649,middle612,
foot575. Actual affected room IDs hex:
`008,00c,018,02f,061,077,0a9,0e1,0e3,0e6,0e7,0f0,0f1,0fa,0fe,10e,113`.
Both transformed and unflipped uses, all graphics contexts, overwrites and
other uses audited. Final dry-run: **zero eligible updates**.

LOW inventory18 IDs =12 geometric +6 trim;10 geometric observed/proved, two
loaded-only feet `7b2f220c1310dd0a` and `bc58650cd64647b1` remain unauthored.
Six LOW sets: four have all band identities proved somewhere in this corpus,
two have these unproved feet. Loaded-only is not final placement ownership.
Of70 canonical SE wall annotations,24HIGH+10LOW now proved;36 still require
other-family/state provenance (includes the unproved SE foot).
Switch/water/door/progression states, real transition graphics contexts,
overworld and accepted room-wide support graphs for the other316 rooms were
not supplied. This does NOT complete ALL-game/ALL-state coverage.

Authoring profile after SHA256
`99217fa007b1341848e49e472ff26380adb82df680e1c018e2f10e75331b2d0c`.
Reconstructing the pre-task bytes after removing only authorized additions
matches discovery's pinned source digest exactly. Thus HIGH20, four references,
all old arrays, masks, settings, groups, materials, rules, existing roles and
concurrent profile work are byte-exact. Semantic source, backups, immutable
capture profiles and sidecars were not edited. No staging/commits/reset.

## Runtime versus intrinsic success; mandatory gate

Exact current C++ solver adapter compiled for offline replay, no app/runtime
source edits. Replay capture029's complete inputs with the edited research
profile: BG2 solved walls**826→834**, unknown walls**8→0**, structural conflicts0;
mixed-floor unknown/conflict0, unknown doorframe4. Southwest foot placements
(8,38),(9,39),(10,40),(11,41) receive actual floor base0 and the authored0..7
source (`placement_grounded_floor_clamp`). Other intrinsic bands are available
to rendering, but this is not evidence that every placement base/normal/contact
is approved. Upper/lower planes are retained independently; this authoring does
not merge them or newly prove overlap behavior.

Complete-room v2 invoked on the complete linked raw/ALTPAT1, matching collision
doorway graph, pinned research profile and runtime solve, and fixtures with
compiled ALTPHM3 maps. **Gates do not pass:**

|Scope|Result|
|---|---|
|055 fixture|exit1,439/439 support,1391 solved/1 unknown walls,0 conflicts|
|060 fixture|exit1,711/711 support,399 solved/62 unknown walls,1 conflict; planes437/274|
|061 fixture and live029 repin|exit2: `mixed floor datum lacks unambiguous same-plane grounded connectivity`|

061 rejection occurs before a v2 JSON/SVG can be emitted. Do not report that
missing artifact as passing, weaken the validator, or approve based on0 unknown
walls. The complete doorway graph's collision arrays match029 exactly, but
exact-state directed staircase gameplay parity is still unapproved. No fresh
gameplay/camera/occlusion review after these profile edits. Next general runtime
capability: resolve/validate mixed-floor grounded ownership with reproducible
same-plane connectivity, then close doorframe/other structure gates.

## Tests, files, artifacts and live inspection

10 diagonal discovery tests,4 preview tests,23 complete-room validator tests
pass. New failure-class regressions exercise LOW V-flipped dispatch, both BGs,
overwrite rejection, exact applied LOW arrays, full-wall transformed normals,
floor-trim preservation and rejection of missing/conflicted cardinal rise.
Existing ambiguous-owner/priority-only/mask/context veto tests remain passing.
Strict native load→serialize→parse→serialize canonical equality passes;
schema21,235 assets,235 heights,213 normals,98 occlusion arrays.

Owned files: `Profiles/ALttP/alttp-profile.toml`, discovery/verify/test tools,
`alttp_low_diagonal_inspect.py`, `alttp_low_diagonal_verify.py`,
`alttp_diagonal_profile_roundtrip.cpp`, this report and HIGH report follow-up.
No runtime/app build required for these profile/tool-only changes.

Evidence root:
`/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/diagonal-low/`.
`discovery.json`, `authoring-verification.json`, `before-profile.toml`,
`authoring.patch`, `current/` final dry-run, `fixtures/` pinned profile/runtime/
ALTPHM3 and055/060 v2 JSON/SVG; `live029/` complete replay inputs, research
profile, runtime JSON, map parity, graph/state repins. Original raw corpus stays
in `diagonal-sets-priority/`; all original SHA pins are checked when reused.

For **live inspection only**, View > Load Remaster Profile… → repository
`Profiles/ALttP/alttp-profile.toml`. This loads the newly fixed metadata; it is
not the root cause of the omission and not room approval. Immutable029 packets
retain their old embedded metadata. Fresh captures must be made from live play,
not by overwriting packet sidecars. Smallest next action is resolve the mixed
floor validator rejection, then fresh matching southwest/southeast gameplay
inspection across camera positions; room061 remains incomplete until all gates
actually pass.
