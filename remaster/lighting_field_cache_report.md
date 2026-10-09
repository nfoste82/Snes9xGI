# Incremental lighting preparation — first stage

## Implemented

- `lighting_field_cache.h` retains effective viewport fields. Exact instance,
  metadata, material, pixel ownership and coordinate-scale comparisons invalidate
  affected pixels. Removed pixels reset their fields. Emission/depth are refreshed
  independently; authored and placement geometry remain unchanged.
- Existing screen-space mesh patch cache consumes retained inputs. Derived mesh
  normals are restored every frame, including neighbors affected by geometry.
- `field_upload_cache.h` retains an exact-byte baseline independently in each of
  the three in-flight Metal resource slots. Only contiguous changed rows are
  uploaded for seven fields and the mesh. First use/resize/device recreation
  uploads complete inputs. No cross-slot version assumptions.
- Visibility-block construction skips unchanged complete inputs in that slot.
  A changed input currently rebuilds the complete visibility grid, not partial
  blocks. Lighting transport still runs every frame.
- Geometry-equivalent artwork reuse implemented separately in
  `dungeon_geometry_cache_report.md`; complete placement comparison and exact
  producer refresh precede reuse. Topology/support changes retain full solving.

## Verification

Portable independent original full-field oracle checks all field values and mesh
cells against retained updates through repeated frames, emission animation,
signed bases, flips, yaw, placement relief, normals, occlusion, materials, scale,
scene selection and removed owners. Exact row uploads and lagging resource slots
are independently checked. Optimized and ASan/UBSan runs pass on room062016/018.
Arm64 Release and Debug application builds pass; `git diff --check` passes.

Stationary 180-frame preparation benchmark, -O2, ten warmup frames:

| Capture | Forced full median/p95 ms | Retained median/p95 ms |
|---|---:|---:|
| room062016 | 1.932 / 1.996 | 1.215 / 1.272 |
| room062018 | 2.623 / 2.779 | 1.695 / 1.735 |

These are field-preparation microbenchmarks, not full scene-preparation/GPU/live
FPS measurements. Comparison work, emission refresh and metadata snapshot copies
still occur. Direct transport/shadow sampling quality is unchanged. GPU resource
reuse has compiled application integration and byte-upload tests; fresh live
visual/timing comparison is still required, not claimed by the portable tests.

Commands from repo root (OUT is the approved temporary opencode directory):

```sh
c++ -std=c++17 -O2 remaster/lighting_field_cache_test.cpp -o "$OUT/lighting-fields-test"
REMASTER_FIELD_BENCHMARK=1 "$OUT/lighting-fields-test" "$HOME/Library/Application Support/Snes9x/Remaster/frame_room062_016.s9xrmf" "$HOME/Library/Application Support/Snes9x/Remaster/frame_room062_018.s9xrmf"
c++ -std=c++17 -O1 -g -fsanitize=address,undefined remaster/lighting_field_cache_test.cpp -o "$OUT/lighting-fields-sanitizer"
"$OUT/lighting-fields-sanitizer" "$HOME/Library/Application Support/Snes9x/Remaster/frame_room062_016.s9xrmf" "$HOME/Library/Application Support/Snes9x/Remaster/frame_room062_018.s9xrmf"
```

`S9X_REMASTER_FULL_FIELDS=1` forces full surface-field preparation for controlled
comparison. Existing `S9X_REMASTER_FULL_MESH=1` forces full mesh reconstruction.

## Next stages

Retained classification/dependency indexing and component-local room updates,
including pickup topology, remain future work. Room-space static meshes are not
introduced. Dirty rows rather than rectangles keep the initial upload design
simple; tracking dirty GPU visibility blocks and reducing field comparison/copy
work are additional opportunities. Mandatory room-height approval remains blocked
by existing complete-room validation failures; this optimization supplies no new
height guesses and does not waive those gates.
