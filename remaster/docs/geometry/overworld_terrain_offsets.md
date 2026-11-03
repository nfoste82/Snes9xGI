# Sparse overworld terrain deformation (profile38 / frame42)

## Per-pixel extension (profile39 / frame43)

`normal_offsets` optionally stores exactly 192 finite floats: 64 world-oriented
unit XYZ support normals. It is mutually exclusive with uniform `normal_offset`.
The runtime selects the per-pixel rotation after undoing artwork H/V indexing,
then rotates the authored detail normal. Debug and inspector result previews use
the same world sample. Schema38 uniform normals and frame42 remain readable.
Inspector whole-tile XYZ assignment replaces a per-pixel field with a uniform
orientation; varying fields display Mixed. Per-pixel painting is not implemented.
Height Offset now defaults to **Per-pixel support offset**. Select a world-oriented
pixel in the upper 8×8 preview and enter a fractional value (Return), or use ±1.
The value is scalar + terrain adjustment, excluding local relief; entering40
stores `40 - scalar` for that pixel. Other pixels, authored local geometry and
normal offsets are preserved. Undo/profile save apply. The alternate **Tile scalar
base** mode retains whole-coordinate scalar editing. Editing height does not
automatically regenerate normal offsets; generated proposals derive those offline.
Numeric pixel editing is supported; drag-to-paint is not implemented here.
Both offset categories support **Copy Layer / Paste Layer** using the selected
representative coordinate as the source and the complete current type/location
selection as destinations. Samples stay world-oriented (no artwork flip).
Height Offset copies scalar plus pixel adjustments regardless of numeric editing
mode; a zero combined field clears both. Normal Offset copies uniform/pixel
rotations; +Z identity clears the normal channel. **Remove Height Offsets** clears
both height channels; **Remove Normal Offsets** clears all normal rotations.
Each preserves the other offset layer and local authored geometry, supports Undo,
publishes the preview once per batch, and requires Save Profile to persist.
Authoring derives pixel normals from assembled, surface-aware height gradients,
not independent 8×8 gradients or one average per tile.

**Overworld authoring rule:** per-pixel height and normal offsets are permitted
only on assets explicitly marked `tile_role = "floor"`. Walls, obstacles and all
other/unknown roles retain their authored local heights/normals and receive only
per-tile scalar height bases. Do not subtract local wall ramps per pixel or rotate
wall normals to fit an inferred terrain silhouette.

Village/river normal authoring: `remaster/tools/ALttP/alttp_author_village_normals.py`
consumes the approved terrain and pinned height-export placement policies, checks
the source offsets still match, and writes a separate schema39 proposal. Only
floor coordinates already carrying per-pixel heights are eligible; constant fractional
object bases retain identity rotations (omitted). Ground/bank/water gradients use
same-surface floor neighbors across tile boundaries. Non-floor neighbors never
enter floor gradients. Numeric regressions live in
`test_village_normal_authoring.py` (NumPy required). Bank normals follow the
approved projected field, not a newly inferred physically vertical wall model.

Optional coordinate data extends scalar placement bases without replacing local
asset/context geometry. Missing scalar = 0; missing terrain height = 0; missing
support normal = +Z (identity rotation). Identity records are omitted on native
save. Existing schema37 inputs, including explicit zeros, remain readable.

```toml
[[overworld_terrain]]
world = "light"
background = 1
x = 37
y = 213
# Optional: exactly 64 finite floats, world-oriented row-major 8×8.
height_offset = [0, 0.25, 0.5, 0.75, 1, 1.25, 1.5, 1.75] # shortened illustration, not valid length
normal_offset = [-0.6, 0, 0.8]
```

At least one channel is required. Heights are finite -32767..32767. Normal must
be unit length (squared-length tolerance .002); -Z singularity is rejected.
Coordinates/world/background have the scalar map's bounds and independent keys.
Both channels use the existing explicit authored-placement mode and verified
Light/Dark BG2 coordinate activation. Dungeon/cave/actors remain unaffected.

`height = local + scalar + per-pixel terrain adjustment`, using float samples
through CPU field and mesh inputs. Terrain samples do not mirror with artwork:
canonical tile pixels are converted back into world-oriented sample positions.
Normals first receive ordinary artwork transforms, then the shortest-arc +Z to
support-normal rotation (no additional twist). This preserves angular detail.
No component-wise vector addition or hash-global normal modification occurs.
One support normal is stored per coordinate; curved support can approximate it
with successive tile orientations. Height gradients are not auto-generated at
runtime; offline authoring tools derive and save the orientations.

**View → GI Debug: Normal Offsets** toggles the support-normal diagnostic (also
in the GI cycle). Missing coordinate entries show +Z; unavailable coordinates,
HUD/OBJ and disabled mode show purple. Height Offsets now displays scalar plus
per-pixel terrain adjustment. Neither debug view substitutes authored local relief.

Inspector **Normal Offset** shares whole-tile type/location selection with Height
Offset. Lower preview is transformed authored normals; upper is rotated output.
Enter `x, y, z`, press Return: finite nonzero vectors normalize before atomic
application to selected coordinates. `0, 0, 1` clears only rotation, preserving
any height field. Undo and normal profile save/three-way merge apply. Height Offset
still edits a scalar; per-pixel height painting is not implemented in the native
inspector. Tools can author the fractional field directly in the profile.

Frame42 records sparse per-instance terrain payloads. Frame40/41 remain readable
with identity terrain. Recorded Evidence remains immutable; Current Profile
Preview may activate new data only on already-verified capture coordinates.
Incremental field invalidation includes both height samples and normal rotation.

Kakariko test authoring: `remaster/tools/ALttP/alttp_kakariko_terrain.py`.
It smooths selected floor support using the user's stepped scalars and nearby
authored wall endpoints, preserves detail/local arrays, and outputs a separate
proposal. This is localized reference-guided authoring, not whole-world terrain
approval. Missing local geometry and excluded mixed props remain evidence gaps.
