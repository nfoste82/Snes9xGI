# Signed authored heights

Authored and placement-local 8×8 height layers use signed 16-bit integers:
**−32768 through 32767**, 128 bytes per layer. Ordinary, facing-specific and
flip-specific layers all share this representation. Opacity, normals and
emission remain byte-valued layers.

The Pixel Inspector Height layer accepts signed values in its numeric field,
slider, pixel painting, tile fill, animation copy and ±1/±8 tile operations.
Decrease crosses zero; saturation occurs only at the int16 endpoints. Undo and
profile save retain signed samples. The numeric field permits an intermediate
minus sign while typing. Local grayscale previews expand their range for
negative/wide layers; NPC height display ranges also accept signed values.

**Tile −1 / −8 / +1 / +8** adjusts all 64 samples on every selected tile,
using each tile's own height layer rather than copying the focused tile. Repeated
identities are adjusted once. The active flip-specific layer or named context is
respected; absent named contexts never fall through to shared geometry. One Undo
restores the entire selection. Fully saturated selections create no undo entry.

## Units and formats

Physical height remains `(placement base + local sample) / 255 * coordinate_scale`.
The denominator is calibration, not a storage limit. Placement bases remain
signed int32 and their existing unknown-base sentinel 255 is independent of
local samples: a local sample of 255 is a valid height.

Profile schema **27** accepts signed height arrays (exactly 64 integers). Older
profiles remain readable with their original 0..255 validation. Saving a layer
outside that legacy range automatically upgrades the profile to schema 27.

Frame schema **35** writes each local sample as explicit little-endian int16,
including placement overrides and facing/flip variants. Previous captures read
their original unsigned-byte samples without reinterpretation. GPU height fields
remain RG32Float; their memory size and lighting coordinate scale are unchanged.

## Verification

- `remaster/signed_height_test.cpp`: signed endpoints, values above 255,
  legacy-schema rejection, overflow/fraction rejection, profile and capture
  round-trips, variants, placement composition, lighting-field cache invalidation,
  inspector values and legacy sample decoding.
- `remaster/dungeon_semantic_cache_test.cpp`: full-byte cache identity for
  positive/negative relief with identical low bytes.
- `tools/ALttP/alttp_signed_dungeon_test.cpp`: signed geometric/support cases.
- Existing inspector, NPC, layer, facing/flip and dungeon regressions.
- arm64 Debug application build passed; signed end-to-end regression passed
  ASan/UBSan. The broad dungeon run passed 41 tests; its sole stale schema
  assertion was updated and that regression subsequently passed. Nine tests
  requiring external fixtures were not run. Relevant Python suite: 112 tests,
  three skipped.
- Pinned room055/060/061 complete-room validation-v2 evidence and runtime outputs
  match pre-migration results. These fixtures retain their existing missing
  wall/conflict counts; this storage migration does not establish new room coverage.

No fresh gameplay visual comparison or exhaustive room/state corpus was performed.
The portable omnibus `remaster_test.cpp` reaches its fixture-count assertion
(`assetGroups.size() == 12`) against the edited working profile; dedicated format
and geometry regressions cover the migration independently.
