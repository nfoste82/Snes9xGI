# Room071001/007/008 — reviewable but incomplete; Release updated

Final current gate/status for the runtime-height follow-ups. No profile, authored
array, Door payload, recovery report or global rise change. Shared dirty work
preserved; no commits. Prior reports retain historical before-state numbers.

##256 actual missed BG1 wall pixels: repaired

Own007 BG1 cells(26,47..50), hashes `b34fbe48662d1787` /
`17bb978913879936`, H/V-flipped usages, have base16 and canonical local0..7.
Complete outer door-band final ROM words plus both adjoining wall endpoints
prove ordinary wall ownership in the existing classifier. Solver classified
these Wall and solved16. Frame application then reapplied hash-wide Door role
from the immutable authored metadata and demanded a door-region attachment,
discarding all256pixels' solved base. No stale cache/VRAM/camera mismatch.

Shared repair: retain `producerOwnedWall` for exact ROM-owned placed Wall usage,
through solved maps/cache inventories. After exact producer identity+flip checks,
this placement provenance takes precedence over asset-wide Door semantics and
stale region claims. Other Door usages still require proven region ownership.
Authored local profiles, roles, normals, masks and emission remain untouched.
This applies both BGs, main/sub/lamp-free scene, not room071 or either hash only.

Own007 now applies8992/8992 positive solved BG1 wall pixel usages, zero unapplied
(previous8736/8992). North16/8/0 and southern40/32/24 remain correct. Dedicated
`dungeon_owned_wall_projection_test.cpp` covers both BGs/buffers, four flips,
signed bases, unowned Door-region behavior and stale-producer rejection.
Complete ROM-band ownership/overwrite rejection remains in the existing
classifier; this consumer does not infer wall ownership from the tile hash.

## Four BG2 conflicts: repaired as empty-art exclusion, not height fill

Exact border-fallback trace located conflicts at(14,22),(17,22),(14,43),(17,43).
These final BG2 words are01ec: completely transparent4bpp graphics clearing
the stairwell so BG1 art is visible. Collision still describes both independent
gameplay planes. The identity-less immediate-border fallback tried to give
nonexistent artwork support on both planes and declared ownership conflicts.

Shared fallback now explicitly excludes completely transparent Unknown artwork
from obstacle/ramp generation; independent movement/support arrays stay intact.
No zero-height geometry or selected nearby plane is fabricated. Visible
ambiguous artwork still conflicts. Artwork-equivalence cache additionally
rejects blank↔nonblank changes, even with otherwise equal/missing metadata,
because transparency is now an explicit geometry dependency.
`dungeon_transparent_border_test.cpp` covers both BGs/four flips, preserved plane
supports, no blank fallback and visible two-plane ambiguity rejection.
Runtime structural conflicts001/007/008:4→0; v2 conflict records1→0.

##264 remaining walls: investigated; explicit evidence gap

All264 own007/008 unknown BG2 walls occupy the southeast assembly, bounds
x34..61,y36..61. All have authored local geometry (260span0..7,4span0..6),
and264/264 corresponding final words match the independent reference snapshot.
They are not missing height arrays or wrong graphics identities. Attributes
are184placements[plane0=01,plane1=00] and80[04,00]. The visible upper shell is
solid on plane0 and nonblocking over independent plane1 support. The solve
cannot use plane1's isolated cap datum as proof of the visible plane0 shell.
Adjacent cleared BG2 interiors01ec are Unknown, not classified upper floor.
The surrounding legacy ceiling local60 is intentional authored content, not
permission to normalize the wall shell to60 or copy the underpass base.

Last writers are default-layout dispatches2,4,6,8,22,26,30,34.42members are
independently identified subtype2 high outer corners0/1/2/3; remaining222 are
straight high perimeter members in the default layout. Metadata/ROM availability
and final map agreement are established, but no accepted high-side/low-side
support/assembly path anchors the plane0 southeast shell in this pinned state.
Missing capability: contextual cross-BG high-perimeter ownership/contact
transport for these disconnected visible upper-shell assemblies, including
the overwrite that clears BG2 interior and the matching BG1 surface. Existing
paired-identical-wall/complementary-face rules do not prove that relationship.
No whole-object offset, blanket plane1 base, nearest-floor fill or ceiling
clamp was added. Per-placement evidence is in `unknown071-class-inventory.json`.
Own001 additionally retains6BG2 unknown walls (270total); BG1 unknown30 remains.

## Final tests, corpus and mandatory gates

All13 targeted portable tests passed: owned-wall projection, transparent border,
stair assembly support, authored stairs, cross-BG wall support, geometry,
geometry cache, classification-plane cache, animated footprints, obstacle
assemblies, authored obstacles, signed pit, clipped corners. Own007 production
EndFrame static checks passed. Room060 own085 three real ROM animation phases
repeated: all8pedestal members base24, exact cached/full effective packets.
Authored relief/pedestal metadata unchanged.

Final audit:234local indoor packets (one new own060092 appeared in the shared
capture directory),63replayed,162missing complete evidence,9stale refused.
Nine packet solve summaries change (transparent-conflict exclusions);12effective
packets change, including restored wall-band application in060 captures and071007.
All346offline contexts/319rooms checked;245effective inventories change due to
explicit transparent fallback exclusions. Aggregate BG2 solved/unknown walls
unchanged89638/256672; structural conflicts2019→1983, no increased contexts.
Room017, borrowed graphics parity and unenumerated dynamic states unsupported.
Transparent placements are explicit non-geometric artwork, not solved geometry.

Mandatory full-input v2 rerun with pinned ALTPPD1, matching ALTPAT1, own profiles,
observed stair edges, per-plane placement bases/effective sources/fallbacks and
generated ALTPHM3 maps. Current results:

| state | valid/required support | plane denominators | missing | conflicts | BG2 unknown walls | exit |
|---|---|---|---|---|---|---|
|071001|297/297|52,245|0|0|270|1|
|071007|465/465|52,413|0|0|264|1|
|071008|297/297|52,245|0|0|264|1|

Those are graph-scoped reachable counts, not whole-room completion: runtime
support scope[492,3289]/[492,3279] is larger; internal subtile door/special
edges and disconnected entries remain incompletely enumerated. No complete-room
approval or capture-ready claim. Reference stair playback is not fresh Snes9x
visual parity. Fixtures055439/439exit1(16unknownwalls),060711/711exit1
(remaining conflict),061exit2(preexisting independent diagonal ROM proof gap
layer1cell2,38). Validator rules were not relaxed.

## Release build and files

Verified `build/release/` existed before building. Ran:

```
xcodebuild -project macosx/snes9x.xcodeproj -scheme snes9x \
  -configuration Release -destination "platform=macOS,arch=arm64" \
  CODE_SIGNING_ALLOWED=NO build
```

BUILD SUCCEEDED; project produced universal arm64/x86_64 runtime framework and
copied it into **build/release/Snes9x.app**. Bundled framework SHA256:
`34858459317877ba65dfcdb329cba22a7ffec2548e5aa2019847e62a72baad6a`.
No app launch/interactive visual approval performed. A running older app still
needs quitting/relaunching to use the new binary. `git diff --check` passed.

Changed this pass: `frame.h`, `remaster.h`, complete-map cache inventory,
cache/capture regressions, two new failure-class tests, audit packet-difference
reporting and these reports. Artifacts remain at the prior
`runtime-height-bugs/stair071-followup/` path: `corpus.json`, `projection-after.*`,
per-capture `gate.json`/validation-v2 JSON+SVG/maps, unknown-class inventory.
No source profiles or rules mutated; no commits.
