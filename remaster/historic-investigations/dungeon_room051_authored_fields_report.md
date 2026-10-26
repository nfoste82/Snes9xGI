# Room051 lower walls: authored metadata versus production fields

## NEW045 follow-up: receiver omission reproduced (2026-10-22)

### Implemented shared repair and verification

`S9xRemasterRestoreSceneSubscreenWalls` now normalizes the complete frame before
live placement application and when decoding older captured frames. It copies an
observed subscreen pixel into scene **ownership and color together**, only when
both scene and main are actual backdrop/no-instance. Eligibility requires verified
USA ROM, BG1 physical Wall/WallCorner effective role, authored effective height and
normals, exact captured per-instance palette artwork, and a non-Unresolved/non-
Conflict occurrence. Unknown/color-math/lamp-mask-only art, missing palette,
contextually absent layers, HUD/main contributors and existing scene pixels are
not replaced. The selected canonical tilePixel retains flips without transforming
twice. No BG raster masks, profile arrays, placement bases or solver rules changed.

NEW045 recovers **4,294** wall pixel receivers; total scene authored wall coverage
becomes9,736, with zero receiver omissions or suppressed fields. NEW044 recovers
4,118. Their original packets/sidecars remain unchanged on disk; repaired ownership
is materialized on load and emitted by future production captures. Repaired fresh
production replay matches142,686 BG samples (138,392 prior +4,294 restored); the
complete-map geometry inventory is byte-identical before/after. Thus this is real
effective local geometry/normal/occlusion restoration, not a diagnostic-only tint,
but it does not establish corrected absolute placement for the still-unresolved
BG1 graph. Existing bases remain unchanged.

`scene_subscreen_wall_test.cpp` covers all H/V combinations, exact canonical color
and relief/base composition, idempotence, wire roundtrip, existing-scene priority,
HUD, main contributor, wrong source/ROM, malformed pixel, missing height/normal/
palette, mask/unknown role, contextual layer absence and resolved non-wall role.
Explicit Unresolved/Conflict reject restoration; a conflict applied after recovery
still suppresses height/normals/occlusion. Tests pass normally and under ASan/UBSan,
including pinned044/045. Occurrence-geometry and lamp plotter/lifecycle/wire tests
pass. Lamp test requires `-ffunction-sections -fdata-sections -Wl,-dead_strip` to
discard unused renderer variants; the initial bare link failed missing globals.

Reaudited **341 indoor packets /34 room IDs**, all pass the new receiver assertion;
the original340-packet corpus had exactly two omissions/8,412 pixel usages, now
zero. One newly arrived061072 packet also passes. Complete-room v2 rerun still
exits1 with the unchanged counts below. Existing055/060/061 fixture runner was
attempted: old obstacle candidate profile rejects duplicate asset-group ownership;
with pinned045 metadata,055 validates439/439 supports but exits1,060 has711missing
supports/one conflict, and061 validator rejects diagonal preview(2,38) lacking ROM
proof (runner does not supply ROM). These are unsupported publication gates, not
passing room fixtures. No solver change was made.

arm64 macOS **Debug and Release builds succeeded**; `git diff --check` passes.
Parent's independent native texture/cache checks (including ASan/UBSan and exact
M3 Max texture upload/readback) are recorded in
`build/room051045-consumer-checks.md`; those checks preceded this normalization.
Fresh interactive revised-build camera/plane validation remains pending. Room051
remains **reviewable but incomplete**, not a successful complete-room height map.

Room051 / own045: **reviewable but incomplete**. The missing production-field
class is now reproduced, without changing solver outputs or authored arrays.
The earlier verdict below applied to occurrence activation only and missed a
receiver-selection denominator: authored walls absent from the scene plane were
not enumerated by that audit.

Exact pinned capture SHA256
`9106a241f209b6594c13a2ca883080679b5dc7bae7bc473c3e2837fc83efa981`;
profile SHA256 `3704edce2c4aaa8ac1591f4a8b1cd65f4130edac9f237afb9fa29fbe9c7a8e54`,
serialized FNV `e6fd563fa0a85504`; complete input SHA256
`7dbd8f7f48382563c665fbf36ba4a7874cef43849ad443828baab02bd676801f`.
ROM remains the USA fingerprint below, settled module7/submodule0, plane0,
world Link(760,2842), scroll(640,2734), configured rise50, authored stair rise24.

### Failure stage and exact denominator

* The stock main plane has backdrop pixels whose real subscreen contributor is
  authored BG1 wall geometry. Existing non-scene field selection correctly uses
  that subscreen receiver. The independently captured scene omits these BG1
  contributors and field selection explicitly disables subscreen fallback when
  `useScene` is true (`lighting_field_cache.h`). The lamp raster path restricts
  BGs to BG2/OBJ (`gfx.cpp` mask0x12); this is a possible producer route, not an
  attested lamp-state claim from sidecars (lamp state was not retained there).
* own045 scene has14,834 backdrop/no-instance pixels, all invalid height. Of
  those, **4,294** have a real authored BG1 wall subscreen contributor with valid
  height/normals/occlusion in non-scene mode but **all three fields missing in
  scene mode**. Affected hashes/pixels:83ff6d16868aaa72/640,
  9214aaf974182610/2438,9836d0b8491c70b9/128,9a0d6f70c687e95c/128,
  a3b3accbe0e35dca/128,f8dd3cd336d542e0/832.
* These are Inherit occurrences. Neither explicit unresolved deactivation nor
  missing canonical metadata explains the loss. The scene's5,442 authored wall
  receivers all retain fields; the non-scene denominator is9,736 with zero
  suppressed fields. Hash-only/selected-scene-only coverage hid the omission.
* Fresh current production EndFrame replay matches138,392 complete-input BG
  pixel/flip/camera samples and reproduces the same4,294 missing receiver fields.
  Use the generic capture probe; the older Room051 height probe has a fixture-
  specific assertion about upper-floor usages that fails on this viewport and
  must not be reported as a passing general regression.
* Parent independently proved retained/fresh cache fields equal for044→045→044
  and production Metal tests1393/1393; no cache/upload repair is justified.

### Scope, gate and next shared capability

Updated read-only audit supports `--main-plane` and explicitly inventories
authored main/backdrop-sub receivers replaced by scene geometry. All46 Room051
packets still pass the old occurrence-activation assertion, demonstrating why
that assertion alone is insufficient. New receiver-omission audit across
**340 available indoor packets /34 room IDs** finds exactly two affected packets:
own0444,118 pixels; own0454,294 (8,412 pixel usages total). This is a capture
corpus, not all320 room states; complete-room BG1 geometry remains unsupported.

Artifacts: `build/room051-wall045/` contains pinned replay manifest, complete
ALTPPD1/ALTPAT1, runtime solution, compiled ALTPHM3, full static observed-seed
doorway graph, v2 JSON/SVG and gate; generic production replay and per-packet
field inventories; `all-room-capture-omission-corpus.json` and capture corpus.
Fresh validation v2 **exits1**: reachable planes[886,0],862/886 valid supports,
24missing; fallback26/26; BG2 walls564solved/54unknown; one conflict record;
compiled2missing/6mismatches; plane overlap0. No invented stair playback edges:
the exact-collision fixture graph has no attested transitions. BG1 production
solve remains0solved/288unknown walls,419conflict nodes, including three-unit
seam contradictions. Independent gameplay planes remain separate.

The implemented runtime repair above preserves
observed physical subscreen receivers when the independent scene is empty,
without reintroducing lamp-mask-only BG1, changing authored arrays, guessing
floor bases, or overriding unresolved/conflict suppression. Keep owner/presentation
and lighting selection consistent; positive/negative transformed receiver
regressions now audit this omission denominator, not only remaining scene pixels.
Restoring these local fields will not solve the BG1 seam/placement-height gap.
Fresh interactive camera/plane comparisons remain necessary after any repair.

Verdict: **blocked by missing reproduction evidence**, not a completed wall repair.
The lower BG1 structural graph remains incomplete. No runtime solver, profile,
authored arrays, or lighting-consumer behavior was changed by this investigation.

## Findings

The suspected consumer path exists: `S9xRemasterResolveInstanceGeometry` and
`S9xRemasterResolveFrameGeometry` deactivate BG height/normal/occlusion on explicit
Unresolved/Conflict occurrences, and `RemasterLightingFieldCache` excludes such
occurrences from absolute geometry. But **this path is not active for the reported
own039 walls**: their occurrence status is Inherit. Do not remove that safety gate
on the basis of this capture.

Fresh current-source production EndFrame replay of own039 matches all 92,826
BG identity/flip/camera samples. All 14,872 visible authored wall/corner pixel
usages retain effective height and normals and valid height/normal/occlusion
fields. Suppressed authored wall pixels: zero. Own044 production replay matches
147,240 samples; its independent retained scene has 5,540 authored wall pixels,
zero suppressed. Neither result proves correct placement bases.

The authored occlusion flag on these wall assets is generally absent; production
uses the normal opaque coverage default (255), not a missing occlusion field.
BG1 wall geometry remains 0 solved / 288 unknown with 419 conflict nodes in own039,
including the previously documented three-unit seam inconsistency. This is a
real effective-height coverage defect, separate from metadata activation. BG2
retains 564 solved / 54 unknown walls and four structural conflict nodes.

The first diagnostic draft incorrectly compared sub-buffer metadata to main
lighting fields. The final audit exactly selects the renderer's scene plane or
main/backdrop-sub fallback and never treats an arbitrary sub pixel as the receiver.
Purple corresponds to height-field validity zero in the Metal shader, not merely
an unresolved floor-base byte in a solver map.

## Regression tooling and scope

Added `dungeon_authored_wall_audit.cpp`: production field builder, per-identity
canonical/effective/field activation, placement override/base, context and status.
`--require-authored-wall-fields` fails when an authored visible wall loses height,
normals, or occlusion validity; it does NOT assert solved support ownership.

All **45 available Room051 gameplay packets** pass that activation regression,
including own039–044. This is one room's captured-state corpus, NOT the previous
346-context/319-room complete-map corpus, which was not reaudited here. Additional
room IDs, unobserved room states, camera positions and BG1 structural coverage
remain unsupported denominators. Existing occurrence-geometry regression and
lighting-field mutation/full-build equivalence pass for fresh own039/044 outputs.
`git diff --check` passes. Only audit tooling/report added; no runtime change or
app build was required/performed. No new interactive gameplay/Metal readback.

## Required validation v2 (fresh run, both fail)

Artifacts: `build/room051-lower-wall-investigation/`, plus its
`own044-validation/` directory. Complete matching ALTPPD1/ALTPAT1, pinned profiles,
state, full doorway graph from actual seed (no invented stair playback), runtime
solution, compiled ALTPHM3, v2 JSON/SVG, and hashes are retained in gate/manifest.

* own039: validator exit1; reachable planes [0,360], 360/360 valid; fallback4/4;
  walls564/54 solved/unknown; compiled4 missing/54 mismatches; overlap0.
* own044: validator exit1; reachable planes [886,0], 862/886 valid (24 missing);
  fallback26/26; walls564/54; compiled2 missing/6 mismatches; overlap0.

ROM SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Own039 serialized-profile FNV `3b98a54cc03c23ea`; own044 `889c3c31f7af7481`.
Full profile/input hashes are in the manifests. Rise remains configured50, signed
authored stair relation24; plane arrays remain independent. No room approval.

## Reproduction / smallest next action

```
c++ -std=c++17 -O2 remaster/tests/dungeon_authored_wall_audit.cpp -o build/room051-wall-audit
build/room051-wall-audit /path/to/frame_room051_039.s9xrmf --require-authored-wall-fields
```

Capture a fresh packet **while the lower wall is actually purple**, preserving
its exact selected wall coordinate/identity, profile, complete input sidecars,
and debug screenshot. Compare native Metal field readback against this audit;
all supplied packets pass the CPU field activation gate, so neither missing
metadata nor explicit unresolved occurrence deactivation explains the reported
visual regression yet. Separately, closing the BG1 graph requires per-face seam
ownership/constraints or authorized canonical-array correction, not guessed
offsets or re-enabling unsolved geometry globally.
