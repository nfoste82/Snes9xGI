# Exact scrolling construction work

Room055 native internal-joypad left/right acquisition (October 28, 2026) proves
both instance-count full selection and all-offset local selection during camera
scroll. Count/ID invalidation is **not** removed. The observed aligned overlap
also contains physical changes and domain-adjoining changes; neither arbitrary
domain remapping nor blind BG/OBJ/HUD translation is justified.

Two shared construction optimizations preserve those contracts:

* `lighting_field_cache.h` computes the eight presence bits once per resolved
  occurrence per invocation. The existing shared selector still examines all
  preferred and fallback candidates, performs scene conservation and handles
  terminal OOB claims. Material presence remains conditioned on metadata presence.
* `surface_mesh.h` stops looking for ramp continuation once continuity is proved.
  At an interior mesh vertex, three or four existing edges of the four-cycle
  prove one connected component; sum the same corners in the original order,
  including initial positive zero, rather than repeatedly resolving union roots.
  Other topologies and viewport borders retain the original generic weld pass.
  Domain, sheet, coverage, masks, continuation and three-pixel cache halos remain
  unchanged. No translated reuse or retained room cache was added.

Reusable curated regressions: `surface_mesh`, `surface_mesh_build_work`, and
`lighting_local_selection`. The mesh-work regression can additionally compile
with `REFERENCE_HEADER` pointing to an independently frozen original header.
The local-selection regression compares all fields, donors, claims, samples,
derived normals and mesh against both its frozen reference and fresh full
consumers across transforms, partial overwrites, missing channels, authored zero,
count/index changes, emission, material/HUD and profile/scene switchbacks.

Ignored evidence, commands, exact startup dirty closure, producer sequence,
alternating-order construction-inclusive CPU timings and actual production Metal
Direct/Composite parity are under
`build/remaster-tests/room055-scrolling-optimization/REPORT.md`.
Packet replays benchmark lighting construction, **not** producer EndFrame,
ordinary display FPS, a complete room-cache capability or height approval.

## Final parent gates

Independent review accepted the scoped repair; fresh O2, ASan/UBSan, C++14
selection and independent geometry tests passed. The frozen full+Metal suite
passed **85/85**:
`build/remaster-tests/20261029T052005Z-_jtmdu91/results.json`.
Both Debug and final main Release builds succeeded with production Metal.
The user-test product is `build/release/Snes9x.app`.

Four alternating-order paired runs over 61 actual native left/right producer
packets measured construction-inclusive fields+mesh **7.160 → 5.688 ms**
(20.6% reduction); mesh alone **1.683 → 0.954 ms**. Complete CPU output bytes
matched the frozen baseline and fresh-full consumers. This is offline sampled
construction evidence, not an ordinary-live speedup or total CPU+GPU budget.
Additional actual-renderer GPU comparisons passed for all **61 native movement
packets plus four switchback draws**, across baseline, candidate and forced-full
paths (195 production draws). Actual scene upload, transported Direct, Composite
and complete field/mesh products matched exactly. Results:
`build/remaster-tests/room055-scrolling-optimization/moving-gpu/results/summary.json`.

## Material lookup follow-up

A separately measured invocation-local material-pointer index removes repeated
per-pixel material-name tree searches without changing material selection or
cache invalidation. It preserves LAST duplicate material/field metadata, FIRST
HUD metadata, absent default material names, explicitly authored empty selectors,
missing names and authored zero reflectance. Setup runs after the unchanged-input
early return; no pointer persists across updates.

Four alternating paired runs on the same 61 native producer packets measured
fields **4.685 → 3.451 ms**, inclusive fields+mesh **5.588 → 4.408 ms** (21.1%).
These use their own paired accepted baseline; do not add percentages or subtract
timings across separately measured stages. Unchanged updates were approximately
0.47 ms in both variants. All complete CPU products and 195 production GPU draws
matched baseline and forced-full paths. Independent review accepted the change
after fresh O2, sanitizer and C++14 tests. Evidence:
`build/remaster-tests/room055-fields-followup/REPORT.md` and sibling
`room055-fields-followup-review/REPORT.md`.

The follow-up Debug and final Release builds passed. The parent full+Metal gate
passed **86/86**, manifest
`build/remaster-tests/20261029T053700Z-ovkik2w3/results.json`.
This closes scoped implementation acceptance. Final user-test product is
`build/release/Snes9x.app`; it was not launched after the final build. No live FPS
or 5 ms combined CPU+GPU budget result is established.
