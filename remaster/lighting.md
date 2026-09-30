# Pixel-voxel occlusion

Lighting represents each visible, height-known pixel as a finite axis-aligned
voxel. For screen pixel `(x, y)` and effective physical height `z`, its bounds are:

```text
X: [x, x + 1]
Y: [y, y + 1]
Z: [z - 0.5, z + 0.5]
```

The effective height combines the tile's local height (or placement override)
and instance height offset, then converts the result using the lighting coordinate
scale. Voxel thickness is one source-pixel-equivalent world unit, independent of
that scale. Bounds are not clamped at height zero or the maximum authored height.

Direct illumination, the debug sphere light, visibility diagnostics, and indirect
transport share the same segment–voxel intersection test. Pixel-grid traversal
provides each voxel's XY slab interval; intersecting that interval with the Z slab
completes the ray–AABB test. Only a positive-length intersection attenuates light.
The source and receiver XY cells are excluded to avoid self-shadowing. Exact XY
corners advance both grid axes, and boundary-aligned rays use one half-open grid
lane instead of double-counting neighboring cells.

Shading heights describe voxel centers, but surface visibility endpoints must
lie on their outward faces. For a represented, height-known, nontransparent
surface, transport offsets its endpoint along the surface normal by
`0.5001 / max(abs(normal))`. This reaches the unit voxel's outward face with a
small numerical clearance. Both receiver and authored-source endpoints use this
rule in Direct, indirect, and Visibility views. Analytic sphere endpoints stay
exact; shading distances and cosines still use authored surface centers.

Starting rays at centers caused opaque floors and emitter sheets to shadow
themselves: shallow rays intersected neighboring coplanar voxels before escaping
the half-unit thickness. This produced square floor-light footprints and removed
distant torch illumination. Face endpoints fix that without changing blocker
bounds or making raised geometry transparent.

An intersected voxel multiplies visibility by `1 - occlusion / 255` once.
Transparent pixels and pixels without known effective height do not block rays.
The existing very-low-transmittance cutoff remains in use. Surface normals and
light reception are independent of blocker opacity.

This replaces the previous downward-solid height-column model. An opaque raised
crossbar now blocks rays through its actual volume, while rays below or above it
remain clear. Horizontal and vertical jail bars can therefore retain full opacity
and cast shadows without closing the transparent openings between them.

The 8×8 acceleration blocks store the minimum voxel bottom and maximum voxel top
in an RG32Float texture. A ray can skip a block when its height interval is entirely
above or below that envelope. The envelope is not itself solid: overlapping
intervals still require individual voxel intersection tests.

## Validation

- `remaster/indirect_lighting_reference_test.cpp` provides a portable CPU oracle
  for finite voxels, ascending/descending rays, tangencies, fractional coverage,
  missing heights, endpoint exclusion, and world-unit thickness.
- `macosx/remaster-lighting-test.mm` tests the production shaders in float and
  half-float radiance, grid openings, direct/debug/indirect transport, and
  accelerated versus unaccelerated traversal. Continuous opaque-floor fixtures
  cover shallow sphere rays, authored emitter sheets, indirect transport, and
  raised blockers (861 checks); the CPU oracle passes 43 checks.
- The model uses represented visible pixels. Hidden and offscreen geometry, or
  multiple surfaces at the same screen coordinate, require additional geometry
  data beyond the current composited frame.

## Possible future improvement: volumetric light shafts

Grid openings currently produce illuminated regions and shadows on receiving
surfaces. Visible shafts in empty space would require a participating medium,
such as fog or dust, with volumetric scattering and extinction. Reuse pixel-voxel
visibility for medium-to-light connections if that feature is added; it should
remain separate from surface illumination.

## Session debug sphere

The debug light is a spherical area emitter with independent analytic geometry;
it does not need additional scene height layers. Radius describes its physical
extent in all three axes, and height describes its center. Thus center height 15
and radius 10 occupy heights 5 through 25. Color and intensity use the same
emission calibration as authored surface emission. Enlarging the sphere increases
emitting area and softens shadows; radius is not an influence-distance cutoff.

Direct light uses 128 deterministic samples over the sphere's subtended solid
angle. Each direction intersects the sphere to obtain a full XYZ endpoint for
pixel-voxel visibility. Integration weights already include source area, facing,
and distance; receiver facing, roughness, reflectance, and visibility are applied
separately. The light emits in every direction. Receivers inside the session
sphere see an enclosing two-sided emissive boundary, bounded by the existing
transport cap, rather than a singularity or a black interior. Energy is injected
only into Direct; reflected Direct feeds the normal indirect chain.

Composite and Emission show a presentation-only circular projection whose front
surface height is `centerZ + sqrt(radius² - planarDistance²)`. Higher scene
geometry hides that glow. Contribution views exclude it. The sphere remains
session-only and is not stored in profiles or frame captures.

Right-drag moves the sphere in the scene. Holding Command during right-drag
adjusts center height instead: up raises it, down lowers it, with one source
pixel of height per source pixel of vertical travel. Height stays within 0–4096,
the panel updates immediately, and dragging preserves the enabled state.
