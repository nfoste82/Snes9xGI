# Structural regions → numerical heights (v1)

`structural_height_solver.py` is game-independent. Its inputs are explicit filled
surface masks, authored relative-height constraints, and spatial contacts. It
does not inspect artwork, game IDs, tiles, ornaments, luminance, or existing
height data. There is no inferred ornament elevation or nearest-floor fallback.
The input is **authored geometry and height evidence**, not a reconstruction
proof. Confidence records the author's confidence; it is not an automatic score.

## Quickstart for a fresh authoring agent

Save the following exact JSON as `scene.json`. This synthetic 1024×1024 scene is
fully covered by a low face, an arbitrary-axis transition, and a high face.
The endpoint difference is 2 steps × 10 authored units, independent of artwork
span. The entire transition shares one normalization, not one ramp per tile.

```json
{
  "format": "structural-height-1",
  "width": 1024,
  "height": 1024,
  "height_unit": "authored-unit",
  "height_range": [0, 20],
  "nodes": {
    "datum": {"anchor": 0, "reason": "Chosen relative origin"},
    "upper": {"reason": "Same top endpoint across this assembly"}
  },
  "relations": [
    {"kind": "rise", "from": "datum", "to": "upper", "amount": 2, "scale": 10,
     "reason": "Two authored elevation steps"}
  ],
  "surfaces": [
    {
      "id": "low-face", "type": "flat", "priority": 0,
      "geometry": {"polygons": [{"outer": [[0,0],[255,0],[255,1023],[0,1023]]}]},
      "height": {"kind": "flat", "node": "datum"},
      "evidence": "Explicit lower endpoint assignment",
      "reason": "Continuous lower structural face", "confidence": 0.9
    },
    {
      "id": "transition", "type": "transition", "priority": 0,
      "geometry": {"polygons": [{"outer": [[256,0],[767,0],[767,1023],[256,1023]]}]},
      "height": {"kind": "ramp", "low": "datum", "high": "upper",
                 "start": [256,0], "axis": [1,0], "span": 511},
      "evidence": "Lower and upper boundary contacts",
      "reason": "One normalized field over the full assembly", "confidence": 0.8
    },
    {
      "id": "high-face", "type": "flat", "priority": 0,
      "geometry": {"polygons": [{"outer": [[768,0],[1023,0],[1023,1023],[768,1023]]}]},
      "height": {"kind": "flat", "node": "upper"},
      "evidence": "Explicit upper endpoint assignment",
      "reason": "Continuous upper structural face", "confidence": 0.9
    }
  ],
  "contacts": [
    {"id": "low-seam", "a": "low-face", "b": "transition", "boundary": true, "tolerance": 0},
    {"id": "high-seam", "a": "transition", "b": "high-face", "boundary": true, "tolerance": 0}
  ]
}
```

From the repository root:

```sh
PY=/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/visual-context-venv/bin/python
"$PY" remaster/tools/ALttP/structural_height_solver.py scene.json --outputdir /tmp/structural-height-review
"$PY" remaster/tools/ALttP/structural_height_solver.py scene.json --outputdir /tmp/structural-height-review --artworkPNG artwork.png --allow-incomplete
"$PY" -m unittest discover -s remaster/tools/ALttP -p test_structural_height_solver.py -v
```

That existing interpreter has Pillow and NumPy (verified). Any Python 3.9+
with Pillow works. NumPy is optional: mask ownership and coordinate evaluation
use it when installed; a standard-library fallback preserves the same semantics.
No other packages are required. The example output directory is illustrative;
choose an appropriate location when invoking the CLI.

## Scene, coordinates, and masks

- `format`, positive integer `width`/`height`, nonempty `height_unit`, increasing
  finite `height_range: [min,max]`, `nodes`, and `surfaces` are required.
  Canvas dimensions are arbitrary; 1024×1024 is supported directly.
- `relations` and `contacts` default to empty lists. `node_tolerance` defaults to
  `1e-8` in authored height units and must be nonnegative.
- The domain defaults to the **whole canvas**. Optional `domain` uses the same
  geometry format as a surface. It explicitly excludes voids/non-target pixels
  from the coverage denominator. A surface hole alone does **not** exclude a
  coverage obligation: an unfilled hole inside the domain is a coverage gap.
- Coordinates: x right, y down. Heights are evaluated at integer `(x,y)` pixel
  coordinates, with `(0,0)` the top-left sample. No half-pixel offset.
- A `geometry` object contains `polygons`, a `mask` path, or both. A polygon is
  `{"outer": [[x,y],...], "holes": [[[x,y],...], ...]}`. Rings have at least
  three integer vertices, are implicitly closed, and are **filled**, including
  their boundary pixels, using Pillow `ImageDraw.polygon`. Hole boundary pixels
  are removed. Authors should use simple rings: there is no topology repair.
  Coordinates may extend beyond the canvas and are clipped by rasterization.
- Multiple polygons are unioned. Holes subtract only from their own polygon;
  another polygon may fill them (useful for caps). A mask is a same-size image,
  converted to L with nonzero pixels inside. Its path is relative to the JSON
  file. It is an authored segmentation mask, never an artwork intensity source.
  The image mask and polygons are unioned. Use lossless binary L PNG masks;
  no antialiasing, alpha semantics, or rescaling is inferred.

Each surface requires:

| Field | Meaning |
|---|---|
| `id` | Unique nonempty string; no game-specific meaning |
| `type` | `flat`, `wall`, `transition`, or `corner`; semantic label, not a hidden shape rule |
| `geometry` | Filled region(s), holes, and/or explicit mask |
| `priority` | Explicit finite number; highest wins per domain pixel |
| `height` | One expression from the grammar below, continuous over the assembly |
| `evidence` | Nonempty string identifying authoring evidence/provenance |
| `reason` | Nonempty structural interpretation string |
| `confidence` | Finite `[0,1]`; copied to visible solved pixels |

Equal top priorities from different surfaces are rejected as ambiguous even if
their heights agree. Those pixels have no owner/height output and are reported.
A higher-priority cap resolves lower-priority ties only where it actually owns
the pixels. Unsolved higher-priority owners never reveal a lower floor as a
fallback. Regions outside the explicit domain do not own output pixels.

## Signed global node graph (solved first)

`nodes` maps names to `{}` or `{"anchor": scalar, "reason": "..."}`. An anchor
sets a scalar relative datum in the declared height unit, not an artwork value.
Any finite signed heights are allowed. Relations are exact signed differences:

```json
[
  {"kind": "same", "from": "a", "to": "b"},
  {"kind": "rise", "from": "b", "to": "c", "amount": -2, "scale": 8,
   "reason": "Explicit two-step descent"}
]
```

`same`: `z(to)-z(from)=0`. `rise`: `z(to)-z(from)=amount*scale`.
`amount` is signed and `scale` is explicitly authored and positive; there is no
implicit game scale. Optional `reason` is accepted on relations. References must
exist. All numbers must be finite.

Each connected component is traversed with signed offsets. Every cycle and
every anchor is checked against `node_tolerance`; there is no least-squares
averaging. A contradictory component marks **all** its nodes `conflict`, with
null values and diagnostic residuals. An unanchored component marks all nodes
`unresolved`, also null. Independently anchored disconnected components may be
solved; the component list makes disconnectedness explicit. Contacts test their
actual relative alignment if they touch. Even unused unresolved nodes prevent
`complete` status. No node receives an implicit zero anchor.

## Height expression grammar

Expressions carry no arbitrary inline elevation constant. Scalar datums and
endpoint heights reference solved nodes. Slopes and coordinate spans are
explicit authoring parameters, not artwork-derived values. An unresolved node
anywhere in an expression makes the whole surface height unresolved (including
unresolved children of `min`/`max`). Expressions never extend their masks.

### Flat

```json
{"kind": "flat", "node": "upper"}
```

`z(x,y)=z(upper)`.

### Affine plane

```json
{"kind": "plane", "node": "datum", "origin": [100,200], "gradient": [-0.25,0.5]}
```

`z(x,y)=a*(x-ox)+b*(y-oy)+z(node)`; equivalently `ax+by+c` with
`c=z(node)-a*ox-b*oy`. Gradient units are height units per screen pixel.
The origin anchor need not be inside the mask. There is no implicit clipping.

### Boundary-normalized ramp (arbitrary screen axes)

```json
{"kind": "ramp", "low": "lower", "high": "upper",
 "start": [100,200], "axis": [-1,0], "span": 64}
```

The axis is any nonzero vector and is normalized to unit length. `span` is a
positive distance in screen pixels:

```text
t = clip(dot((x,y)-start, normalize(axis))/span, 0, 1)
z = (1-t)*z(low) + t*z(high)
```

`low` is the t=0 node, not necessarily the numerically lower height. East/west
walls use `[1,0]`/`[-1,0]`; north/south ramps use `[0,-1]`/`[0,1]`. Diagonal
vectors work identically. The t=0 and t=1 boundaries are parallel lines
perpendicular to the axis. Reuse this same field across all polygons/tiles of
one assembly. Different artwork spans can share the same height endpoints.

### Corners: min/max of planes or ramps

```json
{"kind": "min", "children": [
  {"kind": "ramp", "low": "lower", "high": "upper", "start": [0,0], "axis": [1,0], "span": 64},
  {"kind": "ramp", "low": "lower", "high": "upper", "start": [0,0], "axis": [0,1], "span": 64}
]}
```

`min` and `max` take at least two recursive child expressions. This yields a
continuous folded field when the children are continuous, with deliberate slope
creases. Shared node endpoints do not by themselves prove matching masked
seams: author the contacts as well. Triangulation and general warped coordinate
fields are not part of v1; no unsafe automatic polygon triangulation is applied.

## Explicit contacts and exhaustive seam validation

Contacts **validate** heights after graph solving; they do not alter nodes,
average pixels, smooth surfaces, or add a guessed endpoint. Each contact names
two different surface IDs, unique string `id`, required nonnegative `tolerance`
in height units, optional `delta` (default 0), optional nonnegative
`max_distance` in screen pixels (default √2), and sample definitions:

```json
{
  "id": "wall-cap", "a": "wall", "b": "cap", "tolerance": 0.001,
  "boundary": true,
  "pairs": [[[15,10],[16,10]]],
  "segments": [{"a": [[15,11],[15,30]], "b": [[16,11],[16,30]]}]
}
```

- `boundary: true` is the recommended easy authoring mode: sample **every**
  visible four-neighbour interface pair between these two owners, oriented a→b.
  Occluded portions and holes are excluded automatically.
- `pairs` explicitly lists `[[ax,ay],[bx,by]]` pixel pairs.
- `segments` samples each side's inclusive Bresenham line. The two lines must
  have equal sample counts; corresponding samples form pairs. Multiple parallel
  segment entries can describe a whole band. No mean-based test or sparse
  endpoint shortcut is used. Definitions can be combined; duplicates are tested
  again and counted again.
- All sample coordinates must be in-bounds integer pixels. The selected owners
  must be exactly a and b and unambiguous. The pair must be within
  `max_distance`. Both heights must be solved. Every sample must satisfy
  `abs((z_b-z_a)-delta) <= tolerance`. Default delta 0 is continuity. An explicit
  signed nonzero delta documents an intentional discontinuity/step, never an
  implicit exemption. Tolerance should reflect actual screen sampling: adjacent
  pixels on a sloping plane can differ even when the underlying field is
  continuous. The test operates at these exact pixels, not extrapolated edges.
- Empty contacts fail. Reports include sample count, failure count, maximum
  absolute residual, and up to 20 detailed failing samples. The maximum is over
  numerically comparable pairs, not wrong-owner/unresolved samples. Failures
  remain visible and do not erase otherwise solved pixels.

The solver independently enumerates **all** visible four-neighbour pairs
between different unambiguous owners. Every such pair must have an explicit
spatially valid contact sample. Missing pairs fail validation (counts and first
20 examples are reported). Sampling only one point on a long seam is therefore
insufficient; use `boundary: true`. Two touching faces require a contact even if
they reference the same height node. Overlap alone is not a seam; the final
visible priority interface is checked. Interfaces involving ambiguous pixels
fail through overlap coverage. Canvas/domain boundaries, holes, and separated
components have no fabricated outside contact. Diagonal-only adjacency is not
automatically a seam obligation; explicit diagonal pairs are allowed.

## Artifacts and exit semantics

For syntactically valid input, **all artifacts are written even when numerical
validation fails**:

| File | Exact meaning |
|---|---|
| `height.png` | L grayscale preview; solved `1+round(254*clip((z-min)/(max-min),0,1))`; unknown 0 |
| `height.f64` | Row-major little-endian float64 heights in authored units; NaN for every unknown/outside pixel; no normalization |
| `confidence.png` | L, `round(255*confidence)` on solved pixels, otherwise 0 |
| `coverage.png` | L labels: outside domain 0, uncovered gap 64, unresolved/nonfinite owner 128, equal-priority ambiguity 192, solved 255 |
| `unknown.png` | L, 255 for all non-solved pixels (including outside domain), 0 for solved |
| `surfaceids.png` | RGB packed ID: `R + (G<<8) + (B<<16)` = 1-based input surface index; 0 means no unambiguous owner |
| `report.json` | Status, denominators/counts, every node/component state, node residuals, surface evidence/expressions, contact failures, missing seams, issues |
| `comparison.png` | Only with `--artworkPNG`: unaltered RGB artwork left, height preview right; exact canvas size required |

Black is an **unknown label**, never the representation of true solved zero.
True zero can be gray 1 or another gray depending on the explicit display range.
Use coverage/float64 data for numerical work; grayscale is quantized. Solved
heights outside the authored display range are still numerical solved values
and remain in `height.f64`; preview values clip but validation reports the range
exceedance and becomes incomplete. IDs remain present for an unresolved owner;
ambiguous owners get ID 0. A failed seam does not magically make either side
unknown: consult report status before accepting any result.

`coverage.solved + gap + unresolved + overlap` is the in-domain denominator;
adding `outside` gives total canvas pixels. Every node has a `solved`,
`unresolved`, or `conflict` state. Surfaces include expression-level resolution
status and unambiguous visible owned pixel counts (an occluded surface can own
zero pixels). Per-pixel nonfinite evaluation is reported separately.

- Exit **0**: complete, or explicitly `--allow-incomplete` for review artifacts.
- Exit **1**: syntactically valid, `review_incomplete`; gaps, overlaps, unresolved
  pixels/nodes, contradictions, missing/failed contacts, nonfinite results, or
  display-range exceedances exist. Outputs are written before returning.
- Exit **2**: `invalid_input`, e.g. malformed expressions, missing references,
  bad coordinates, invalid masks, or nonfinite authoring numbers. `report.json`
  records the error; there is no promise of image artifacts on malformed input.
  Use a fresh output directory per run to avoid confusing earlier images with
  an invalid-input report.

`--allow-incomplete` changes only exit 1 to exit 0; it never changes the report's
`review_incomplete` status or bypasses malformed-input errors. The CLI never
reads optional artwork until after heights are solved. It is used solely for
comparison output and cannot change height, coverage, confidence, or ownership.

## Reusable family authoring without a thousand hand-written regions

v1 JSON is intentionally expanded data, with no executable expressions, hidden
shape IDs, or game-specific instances. Generate it with small Python helpers:

```python
from copy import deepcopy

def wall_instance(name, polygon, start, axis, span, lower, upper):
    return {
        "id": name, "type": "wall", "priority": 1,
        "geometry": {"polygons": [{"outer": polygon}]},
        "height": {"kind": "ramp", "start": start, "axis": axis, "span": span,
                   "low": lower, "high": upper},
        "reason": "Instance of endpoint-constrained wall family",
        "evidence": "Author supplies the two assembly boundary nodes",
        "confidence": 0.8,
    }

def translated_wall(template, name, dx, dy):
    result = deepcopy(template)
    result["id"] = name
    for polygon in result["geometry"]["polygons"]:
        for ring in [polygon["outer"], *polygon.get("holes", [])]:
            for p in ring:
                p[0] += dx
                p[1] += dy
    result["height"]["start"][0] += dx
    result["height"]["start"][1] += dy
    return result
```

Loops can generate repeated faces, caps, all-pair `boundary: true` contacts for
known neighbours, or multipart masks sharing one expression. Translation must
move geometry **and** field origin/start/contact samples; reflection must also
reflect ramp axes or plane gradients. Names can be generated and height nodes
shared across a whole assembly. For nested corner expressions transform every
child. The solver does not silently transform or tile fields. Only emit known
contacts: a blanket contact list creates failing empty contacts for separated
surfaces. Leave uncertain structures explicitly unresolved with evidence gaps
rather than assigning guessed zero or ornamental heights.
