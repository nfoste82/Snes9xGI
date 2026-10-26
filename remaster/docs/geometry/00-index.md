# Geometry and Placement

How authored local geometry is attached to world support and composed into the
physical scene. Commands and inline paths use the repository root unless stated
otherwise. Return to the [primary index](../../../AIDocs/00-index.md).

## Runtime placement and composition

- [HUD-covered Geometry](hud_geometry.md): Independent world ownership beneath BG3, stock color preservation and sprite consistency.
- [Movable Sprite Obstacles](sprite_obstacles.md): Occurrence groups, producer ownership, contact-floor bases and background isolation.
- [Link Attachment Heights](link_attachment_heights.md): Shared support for Link's body, equipment and shadow.
- [NPC Floor Support](npc_floor_support.md): Walkable contact-footprint support and shared follower-body bases.

## Coverage and investigation workflows

- [ALTTP Coverage and World Authoring](../../../AIDocs/alttp/00-index.md): Game-specific reconstruction, validation contracts and reference-guided world authoring.
- [Room Height Auditor](../../tools/ALttP/ROOM-HEIGHT-AUDITOR.md): Tool-first defect discovery, coverage gaps and reusable detection.
- [Room Tile Repair Expert and Agent Roles](../../tools/ALttP/ROOM-TILE-REPAIR-EXPERT.md): Authorized repairs, discovery/repair separation and human-authoring boundaries.
- [Historic Investigations](../../historic-investigations/00-index.md): Matching symptoms, regression evidence and prior repairs; not a current issue list.

Correct height coverage for every room and overworld area is the current priority.
Feature regressions, local height-array presence and viewport captures do not prove
complete-room coverage. Preserve the complete-room validation and evidence standards
in the ALTTP notes. For pixel-layer editing, see [Authoring and Inspection](../authoring/00-index.md).
