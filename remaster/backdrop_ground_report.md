# Outdoor transparent-pixel backdrop ground

`frame_overworld_x0758_y0463.s9xrmf` (schema37, 256×224) has
2,009 main-screen backdrop pixels, with no tile instance and no subscreen
contributor. These include the purple height-view fringe around the church trees.
They all display RGB555 `0x2669`, the surrounding green ground color. This is
transparent artwork revealing CGRAM color zero, not a hidden authored tile or
an ownership/depth-test error. The `_001`, `_002` and `_003` captures retain
the same 2,009 backdrop pixels.

## Repair and boundaries

The lighting field now represents true backdrop-only pixels in verified ALTTP
Light/Dark World captures as provisional height-zero, +Z ground sheets. They
share the existing base-zero provisional BG ground domain, participate in
lighting/mesh construction, and retain `fallbackGround` provenance. This is
not authored/validated terrain or object geometry, nor an inference of raised
terrain support. No profile data, tile identities, ownership, capture schema,
or stock artwork is modified. Indoor, other-ROM, unknown-area, unsupported and
real subscreen tile pixels do not receive this fallback. Independent scene
planes use their own ownership and do not overwrite real contributors.

Clicking these pixels still cannot author a tile: none contributed to the
visible pixel. The inspector now explains the backdrop, provisional base and
need to click opaque artwork. It deliberately does not pick a nearby tree or
invent a decoded tile at a transparent pixel.

## Validation

Compile `remaster/backdrop_ground_test.cpp` with C++17, both `-O0 -g` and
`-O2 -DNDEBUG -Wall -Wextra -pedantic`; optional arguments accept captures.
Checks stay active in Release. Synthetic checks cover cache reuse and area/ROM
changes, both worlds, unsupported ownership, subscreen precedence, independent
scene geometry, missing tile selection and unchanged authored heights.

All four church captures pass: 2,009 backdrop pixels gain provisional ground;
40,518 visible authored height pixels in the original and 44,098 in each later
capture retain exactly their effective signed values.

`remaster/lighting_field_cache_test.cpp`, built `-O2` with assertions enabled,
passes mutation/full-field and mesh equivalence on the original capture. Its
independent preparation oracle includes the backdrop rule. `git diff --check`
passes. Inspector height separation checks and macOS arm64 Debug/Release builds
pass. Fresh GUI verification remains
pending; CPU field/mesh checks alone do not prove final displayed lighting.
