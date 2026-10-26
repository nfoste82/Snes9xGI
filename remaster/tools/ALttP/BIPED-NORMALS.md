# Remaining bipedal NPC normals — review v1

**Status confirmed 2026-10-26:** accepted NPC proposals, including this normals
work, were merged into `remaster/profiles/ALttP/alttp-profile.toml`. Candidate
loading/generation and pending-review language below are historical. Do not
replace the canonical profile with an old candidate; recorded seam and runtime
validation limits remain evidence limits, not pending promotion requirements.

Load `build/remaster-geometry/biped-normals-v1/biped-profile.toml`.
This candidate builds on the edited reference-NPC v6, not the rejected original
humanoid proposal. Main, v6 and the user's previous edits remain untouched.

```sh
python3 -B remaster/tools/ALttP/alttp_biped_normals.py
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_*geometry.py'
```

## Scope and preservation

- 855 tile identities across 58 named families: 413 whole-tile +Y fills replaced
  and 442 missing normal maps added. Existing +X fills are **not** treated as
  placeholders. Detailed normals remain authoritative even when approximate.
- Normal-only changes: no new or replaced heights, sampling options, materials,
  rules, geometry variants or named contexts. Every existing field is checked
  against the input; only authorized `normal_xyz` replacements may differ.
- Link, Ganon (including user shoulders and necklace), Agahnim, Blue/Green/Red
  sword guards and Armos Knights are unchanged relative to v6.
- Includes standing and seated people, robed figures, equipment-bearing guards,
  fairies with humanoid anatomy, skeletons, mummies and bipedal monsters.
- 95 of 950 potentially unauthored identities remain: 24 are blocked by foreign
  actor/background sharing or authored variants; the remainder occur only in
  unverified inherited graphics or deferred boss phases. This is not full coverage.
  `report.json` lists each remaining identity and reason. Blind's detached phases
  are deferred; approved first-phase hashes can still affect reused later parts.

## Modeling lessons applied

Inspect assembled artwork in `inventory-*.png`; names/cell positions are in
`inventory-index.json`. Draw-slot head/body overlap and per-family landmarks
separate anatomy. Heads and torsos keep shared centers across mirrored halves;
only independent limbs split into sides. Per-row smoothed limb centerlines give
sleeves/legs a bent axis rather than one global body cylinder. Tools, the smith's
hammer, lumberjacks/saw, bow and bomb are separated from body extents.

Head crowns, shoulder attachments and boots tend toward world +Z. Do not put an
exposed horizontal cap on the top of every segment: covered trouser tops and
the middle of the chest are not shelves. Pointed hats have a sloped conical
surface; armored faces preserve broader planes with edge curvature. Faces have
shallow cheek/nose relief, separate from hair and caps. Mummy wrappings and
skeleton ribs retain small anatomical relief. Fine eyes, fingers, jewelry and
part boundaries are approximate, not recovered high-resolution anatomy.

Cloth normals derive from continuous draped folds, varying horizontally and
vertically with shoulder attachment and hem position. Sleeves fold separately.
Normal-only folds do not add matching height or occlusion relief.

Brighter pixels supply **bounded supporting evidence** for +Z within local,
similar-chromaticity material regions. White collars/eyes are not compared to
brown faces; dark paint does not become a deep socket. Brightness is not used as
height, and cannot replace the anatomical orientation prior.

## Review and validation

`review-*.png` columns: artwork, anatomical segmentation, exported normals,
neutral-albedo left light, right light and overhead light. `review-index.json`
maps page/row to actor, context and facing. Review sheets cover representative
facings; the exported-map audit examines all modeled unique poses.

`coverage-and-seams.json` records actual exported profile normals, not only
analytical surface functions. Same-group neighbor audit touching changed maps:
172,119 pairs; 19,404 cross tile/placement boundaries; 3,677 exceed45°, including
764 tile-edge pairs. There are398 tile-edge pairs over60° (listed individually).
Some involve overlaps and existing authored/generated joins, but these numbers
are **remaining risks, not a seamless result**. Reused-pixel proposals disagree
on345 identities; semantic mode/median aggregation is approximate. Large shared
hash conflicts need placement/facing-specific geometry rather than flattening
the whole character or pretending the discrepancy is solved.

Of3,714 audited central crown samples,3,471 are +Z dominant. Existing authored
maps and hash-reused placements can prevent the proposed cap from winning.
This does not validate every shoulder, hand or boot top.

The actual C++ profile parser and geometry unit tests pass. Visual review in the
NPC browser and live animation/GI validation are still required. No rebuild is
needed to load this profile. No NPC promotion or git push is performed.
