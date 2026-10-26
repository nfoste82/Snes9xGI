# Link reference-guided geometry proposal

**Status confirmed 2026-10-26:** accepted geometry is merged into the canonical
profile. Generation commands, candidate paths and pre-promotion review requirements
below describe the historical workflow, not a pending promotion. Do not regenerate
or promote an old candidate over subsequent authored edits.

**Approved 2026-10-19:** the user accepted the proposal and its changes have been
merged into `remaster/profiles/ALttP/alttp-profile.toml`. The original candidate/input
snapshot remain available for review. See [Humanoid NPC Geometry](HUMANOID-GEOMETRY.md)
for subsequent NPC proposals and the conflict-aware Link promotion command.

Generate a **separate** loadable profile using the user's existing authored Link
geometry plus explicit part models guided by their supplied 3D Link reference:

```sh
python3 -B remaster/tools/ALttP/alttp_link_geometry.py
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_link_geometry.py'
```

Requires NumPy and the already-generated NPC manifest/runtime profile validator.
Load `build/remaster-geometry/link-geometry/link-profile.toml` through **View >
Load Remaster Profile…**, then inspect Link in **Window > Remaster NPC Poses**.
The source profile is not changed. `source-profile.toml` preserves the input
snapshot; `report.json` records its SHA-256, changed identities and evidence.

`review-*.png` columns are artwork, normals, heights (0–24) and neutral-albedo
directional lighting. `review-index.json` identifies each action/facing row.
These are offline normal-shading previews, **not runtime GI validation**.

## Preservation and coordinate contract

- The user clarified that uniform **+Y**, not +X, was their temporary fill.
  Only whole-tile +Y/+X fills or missing normal maps are eligible.
- All non-placeholder normal arrays, all existing heights and other existing
  asset fields remain unchanged. No other actor or BG-shared hashes are edited.
- North walking frame 1 and other authored walking groups supply physical-part
  and same-facing artwork-patch references. There is no learning/training step.
- X is screen-right, Y is toward the camera, Z is up. Hat crowns and boot tops
  point up; far crown rolloff may point slightly -Y. Blade faces remain planes,
  with separate edge bevels. Outlines/eyes follow neighboring physical surfaces.
- Overrides for vertically flipped tiles store gameplay-space vectors to avoid
  flipping viewer-facing surfaces away from the camera. Pixel arrays stay in
  canonical artwork order. Existing normal transforms are preserved.

## Scope and remaining review

The 2026-10-19 proposal covers 437 unique identities in 1,264 **catalogued** Link
poses. It adds 345 heights and 216 normals, replaces 136 temporary fills, and
preserves 92 height maps and 85 detailed normal maps. 42 tiles receive flip-specific
geometry. This is not an exhaustive inventory of every Link action in the game.

The proposal is deliberately a review candidate. Shared-placement estimates
disagree beyond 2 height units or 30 degrees at 586 pixel/flip samples on 31
tiles. Those are explicitly listed in `report.json`; canonical mean normals and
median heights reconcile them approximately, not as proven geometry. Weapon
elevation depends on pose and remains approximate under hash-keyed metadata.
Some material/outline transitions can still look uneven. The runtime's special
generated-ramp alignment does not apply to arbitrary custom heights; these use
the existing authored part-local scale instead, not whole-pose screen-row ramps.
Live facing transforms, animation continuity, GI and ground placement need
interactive review before promoting the candidate to the main profile.
