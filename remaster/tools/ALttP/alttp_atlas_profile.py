"""Generate a conservative hash-keyed ALTTP lighting profile from an atlas.

The first pass uses decoded artwork and source family, not inferred object
identity. Every decoded tile gets all four pixel layers. Existing authored
values are preserved except for explicitly classified local floor/wall geometry.
Obstacle relief remains authored. Generated values are intentionally simple
and the report identifies ambiguous cross-family hashes for review in game.
"""

from __future__ import annotations

from collections import Counter
import json
import hashlib
import math
import os
from pathlib import Path
import re
import sqlite3
import tomllib

from alttp_tile_semantic_atlas import AnnotationStore, TileCatalog, validate_annotation
from alttp_reference_wall_templates import ReferenceTemplates
from alttp_authored_obstacles import validate_groups


LAYERS = ("height", "normal_xyz", "occlusion", "emission_rgba")
FRONT = (128, 128, 255)
ZERO_EMISSION = (0,) * 256
SECTION = re.compile(r"(?m)^\[\[(assets|rules)\]\]\s*$")
TILE_HASH = re.compile(r'(?m)^tile_hash\s*=\s*"([^"]+)"\s*$')
HUD_GROUP = re.compile(r'(?ms)^(\[asset_groups\.hud_tiles\]\n)tile_hashes\s*=\s*\[[^\n]*\]')

# Ordered from the upper landing to the lower floor in the captured room 0x61.
# Each hash occurs in background graphics pack 1. The profile groups bind the
# same hashes to floor and railing materials; these values describe geometry.
STAIR_TREAD_HEIGHT = {
    "v1:4bpp:ded4f16afe03288a": 39,
    "v1:4bpp:76a7931735f4b282": 27,
    "v1:4bpp:6e4c250f7bad7f96": 15,
    "v1:4bpp:c21e390677b3449f": 3,
}
STAIR_RAIL_SEGMENT = {
    "v1:4bpp:954bb11dcd670e2d": 0,
    "v1:4bpp:99863ac4c1bb8e68": 1,
    "v1:4bpp:865232fba7b0c36c": 2,
    "v1:4bpp:dd29a2e039ea282b": 3,
}
UPPER_GUARD_RAIL_TILES = {
    "v1:4bpp:856f1117785093af",
    "v1:4bpp:810d672fdabc4677",
    "v1:4bpp:a60be53f5ef57310",
}

# Horizontal dungeon-wall decorations above a landing. The four repeated
# column rows and the three crest rows are identifiable artwork, so their
# local vertical geometry can be shared across rooms. Room placement supplies
# the separate lower/upper-floor offset.
WALL_OVERLAY_SEGMENTS = {
    "v1:4bpp:2ad3233860fffd3b": ("column", 0),
    "v1:4bpp:25b414f2340a85a6": ("column", 1),
    "v1:4bpp:ba4694946b93ee92": ("column", 2),
    "v1:4bpp:abe85623ae3f8007": ("column", 3),
    "v1:4bpp:f1555212da242c3e": ("crest", 0),
    "v1:4bpp:7518595e103dbf40": ("crest", 0),
    "v1:4bpp:4c4a9006dd7fa38c": ("crest", 1),
    "v1:4bpp:0c9662bf9776eaff": ("crest", 1),
    "v1:4bpp:2760aea1a605832c": ("crest", 2),
    "v1:4bpp:3a8e21ad63732905": ("crest", 2),
    "v1:4bpp:28159eebb8b262ac": ("crest", 0),
    "v1:4bpp:134e16174f315211": ("crest", 0),
    "v1:4bpp:3387dad63461e82a": ("crest", 1),
    "v1:4bpp:482a1674f4994ba7": ("crest", 2),
    "v1:4bpp:8dbdd042fb53c3ac": ("crest", 2),
}


class ProfileGenerationError(ValueError):
    pass


def _encode_normal(x: float, y: float, z: float) -> tuple[int, int, int]:
    length = math.sqrt(x * x + y * y + z * z)
    return tuple(max(0, min(255, round(128 + 127 * v / length)))
                 for v in (x, y, z))


def _silhouette_layers(indices: bytes) -> dict[str, tuple[int, ...]]:
    """A small camera-facing bulge with a descending per-row height ramp."""
    mask = [value != 0 for value in indices]
    height = [0] * 64
    occlusion = [0] * 64
    normals = list(FRONT * 64)
    rows = [[x for x in range(8) if mask[y * 8 + x]] for y in range(8)]
    columns = [[y for y in range(8) if mask[y * 8 + x]] for x in range(8)]
    for y in range(8):
        if not rows[y]:
            continue
        left, right = rows[y][0], rows[y][-1]
        for x in rows[y]:
            p = y * 8 + x
            height[p] = 13 - y
            occlusion[p] = 255
            # Only visible silhouette edges bend. Tile-border edges may be
            # continued by an adjacent 8x8 part, so keep them camera-facing.
            nx = (-0.48 if x == left and left > 0 else
                  0.48 if x == right and right < 7 else 0.0)
            top, bottom = columns[x][0], columns[x][-1]
            ny = (0.28 if y == top and top > 0 else
                  -0.20 if y == bottom and bottom < 7 else 0.0)
            normals[p * 3:p * 3 + 3] = _encode_normal(nx, ny, 1.0)
    return {"height": tuple(height), "normal_xyz": tuple(normals),
            "occlusion": tuple(occlusion), "emission_rgba": ZERO_EMISSION}


def _neutral_layers() -> dict[str, tuple[int, ...]]:
    return {"height": (0,) * 64, "normal_xyz": FRONT * 64,
            "occlusion": (0,) * 64, "emission_rgba": ZERO_EMISSION}


def _upper_guard_rail_layers(indices: bytes) -> dict[str, tuple[int, ...]]:
    # The dark index 9 is the surrounding wall/air; the painted rail stands
    # above the landing, with its lower rows closer to the landing surface.
    height = tuple(0 if value == 9 else 24 - pixel // 8
                   for pixel, value in enumerate(indices))
    occlusion = tuple(0 if value == 9 else 255 for value in indices)
    return {"height": height, "normal_xyz": FRONT * 64,
            "occlusion": occlusion, "emission_rgba": ZERO_EMISSION}


def _stair_layers(content_id: str, indices: bytes) -> dict[str, tuple[int, ...]]:
    """Model the four visible treads and their smooth paired railings."""
    result = _neutral_layers()
    heights = [0] * 64
    normals = list(FRONT * 64)
    if content_id in STAIR_TREAD_HEIGHT:
        level = STAIR_TREAD_HEIGHT[content_id]
        for y in range(8):
            for x in range(8):
                p = y * 8 + x
                shade = indices[p]
                # Dark stone flecks are shallow pits. Most tread pixels keep
                # the camera-facing +Z normal of a horizontal step surface.
                heights[p] = max(0, level - (2 if shade == 9 else 1 if shade == 10 else 0))
                if shade in (9, 10, 11):
                    nx = ((3 * x + 5 * y) % 5 - 2) * 0.045
                    ny = ((7 * x + 2 * y) % 5 - 2) * 0.045
                    if shade == 9:
                        ny += 0.13
                    normals[p * 3:p * 3 + 3] = _encode_normal(nx, ny, 1.0)
    else:
        segment = STAIR_RAIL_SEGMENT[content_id]
        side = _encode_normal(0.0, 1.0, 1.0)
        for y in range(8):
            height = max(0, 48 - round((segment * 8 + y) * 48 / 32))
            for x in range(8):
                p = y * 8 + x
                heights[p] = height
                # The upper cap is flat; the continuous rail below it faces
                # equally toward the camera and toward +Y.
                if segment != 0 or y >= 5:
                    normals[p * 3:p * 3 + 3] = side
    result["height"] = tuple(heights)
    result["normal_xyz"] = tuple(normals)
    return result


def _wall_overlay_layers(content_id: str) -> dict[str, tuple[int, ...]]:
    kind, segment = WALL_OVERLAY_SEGMENTS[content_id]
    heights = []
    normals = []
    for y in range(8):
        for _x in range(8):
            if segment == 0:
                height = 55 - y
            else:
                height = max(0, 63 - segment * 16 - 2 * y)
            heights.append(height)
            if kind == "column" and segment == 0 and y < 5:
                normals.extend(FRONT)  # Flat top of the column capital.
            elif kind == "crest":
                normals.extend((128, 218, 218))  # Shallow relief on the +Y wall.
            else:
                normals.extend((128, 255, 128))
    return {"height": tuple(heights), "normal_xyz": tuple(normals),
            "occlusion": (255,) * 64, "emission_rgba": ZERO_EMISSION}


def _link_references(connection: sqlite3.Connection, authored: dict[str, dict]) -> list[tuple[bytes, dict]]:
    result = []
    for content_id, asset in authored.items():
        if "height" not in asset or "normal_xyz" not in asset:
            continue
        row = connection.execute(
            "SELECT d.indices FROM decoded_tile d JOIN graphics_source s USING(content_id) "
            "WHERE d.content_id=? AND s.source_kind='link' LIMIT 1", (content_id,)).fetchone()
        if not row:
            continue
        visible_normals = {tuple(asset["normal_xyz"][i * 3:i * 3 + 3])
                           for i, value in enumerate(row[0]) if value}
        if len(visible_normals) >= 5:
            result.append((row[0], asset))
    return result


def _transfer_link(indices: bytes, references: list[tuple[bytes, dict]],
                   fallback: dict[str, tuple[int, ...]]) -> dict[str, tuple[int, ...]] | None:
    visible = [(x, y, indices[y * 8 + x]) for y in range(8) for x in range(8)
               if indices[y * 8 + x]]
    if len(visible) < 10:
        return None
    best: tuple[float, bytes, dict, bool, int, int] | None = None
    for source, asset in references:
        source_visible = sum(value != 0 for value in source)
        for flip in (False, True):
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    exact = 0
                    for x, y, value in visible:
                        sx, sy = (7 - x if flip else x) + dx, y + dy
                        if 0 <= sx < 8 and 0 <= sy < 8 and source[sy * 8 + sx] == value:
                            exact += 1
                    if exact < 10:
                        continue
                    # A partial color match can belong to a different facing.
                    # Compare both masks before copying detailed normals.
                    score = 2 * exact / (len(visible) + source_visible)
                    score -= 0.01 * (abs(dx) + abs(dy))
                    if best is None or score > best[0]:
                        best = (score, source, asset, flip, dx, dy)
    if best is None or best[0] < 0.94:
        return None
    _, source, asset, flip, dx, dy = best
    heights = list(fallback["height"])
    normals = list(fallback["normal_xyz"])
    for x, y, value in visible:
        sx, sy = (7 - x if flip else x) + dx, y + dy
        if not 0 <= sx < 8 or not 0 <= sy < 8:
            continue
        source_pixel = sy * 8 + sx
        if source[source_pixel] != value:
            continue
        target_pixel = y * 8 + x
        heights[target_pixel] = asset["height"][source_pixel]
        normal = asset["normal_xyz"][source_pixel * 3:source_pixel * 3 + 3]
        if flip:
            normal = [max(0, min(255, 256 - normal[0])), normal[1], normal[2]]
        normals[target_pixel * 3:target_pixel * 3 + 3] = normal
    return {**fallback, "height": tuple(heights), "normal_xyz": tuple(normals)}


def _normal_from_height(heights: list[int] | tuple[int, ...]) -> tuple[int, ...]:
    if len(heights) != 64:
        raise ProfileGenerationError("authored height is not 8x8")
    result = []
    for y in range(8):
        for x in range(8):
            left = heights[y * 8 + max(0, x - 1)]
            right = heights[y * 8 + min(7, x + 1)]
            top = heights[max(0, y - 1) * 8 + x]
            bottom = heights[min(7, y + 1) * 8 + x]
            result.extend(_encode_normal((left - right) * 0.75,
                                         (top - bottom) * 0.75, 1.0))
    return tuple(result)


def _semantic_wall_layers(annotation: dict) -> dict[str, tuple[int, ...]]:
    """Build the canonical one-unit-per-pixel wall segment."""
    ramps = {
        "north": [y for y in range(8) for _x in range(8)],
        "south": [7 - y for y in range(8) for _x in range(8)],
        "west": [x for _y in range(8) for x in range(8)],
        "east": [7 - x for _y in range(8) for x in range(8)],
        "north_west": [round((x + y) / 2) for y in range(8) for x in range(8)],
        "north_east": [round(((7 - x) + y) / 2) for y in range(8) for x in range(8)],
        "south_west": [round((x + (7 - y)) / 2) for y in range(8) for x in range(8)],
        "south_east": [round(((7 - x) + (7 - y)) / 2) for y in range(8) for x in range(8)],
    }
    normals = annotation["normals"]
    if annotation["role"] == "wall":
        heights = ramps[normals[0]]
    else:
        if annotation.get("corner_type") not in ("inside", "outside"):
            raise ProfileGenerationError("incomplete wall corner cannot generate height")
        combine = max if annotation["corner_type"] == "inside" else min
        heights = [combine(a, b) for a, b in zip(ramps[normals[0]], ramps[normals[1]])]
    return {"height": tuple(heights), "normal_xyz": _normal_from_height(heights),
            "occlusion": (255,) * 64, "emission_rgba": ZERO_EMISSION}


def _format_layer(name: str, values: tuple[int, ...]) -> str:
    expected = {"height": 64, "normal_xyz": 192,
                "occlusion": 64, "emission_rgba": 256}[name]
    lo, hi = (-32768, 32767) if name == 'height' else (0, 255)
    if len(values) != expected or any(type(v) is not int or not lo <= v <= hi for v in values):
        raise ProfileGenerationError(f"generated {name} has invalid values")
    return f"{name} = [" + ", ".join(str(v) for v in values) + "]\n"


def _render_profile(source_text: str, authored: dict[str, dict],
                    generated: dict[str, dict[str, tuple[int, ...]]],
                    geometry_overrides: set[str] | None = None,
                    tile_roles: dict[str, str] | None = None) -> str:
    """Keep original text and insert only absent fields / new asset tables."""
    parsed_source = tomllib.loads(source_text)
    if any(v < 0 or v > 255 for layers in generated.values() for v in layers.get('height', ())):
        source_text = re.sub(r'(?m)^schema_version\s*=\s*\d+\s*$',
                             f"schema_version = {max(27, parsed_source.get('schema_version', 1))}", source_text, count=1)
    validate_groups(parsed_source)
    # Work on asset/rule sections without swallowing interleaved/appended group
    # member tables. Keep their original text, never reauthor coordinates/Z.
    headers = list(re.finditer(r'(?m)^\[.*\]\s*$', source_text))
    group_blocks = []
    for i, h in enumerate(headers):
        if h.group().strip() in ('[[obstacle_groups]]', '[[obstacle_groups.members]]'):
            end = headers[i+1].start() if i+1 < len(headers) else len(source_text)
            group_blocks.append((h.start(), end, source_text[h.start():end]))
    for start, end, _ in reversed(group_blocks):
        source_text = source_text[:start]+source_text[end:]
    matches = list(SECTION.finditer(source_text))
    asset_starts = [match.start() for match in matches if match.group(1) == "assets"]
    first_rule = next((match.start() for match in matches if match.group(1) == "rules"),
                      len(source_text))
    if asset_starts and asset_starts[0] >= first_rule:
        raise ProfileGenerationError("profile assets follow rules unexpectedly")
    prefix_end = asset_starts[0] if asset_starts else first_rule
    output = [source_text[:prefix_end].rstrip() + "\n\n"]
    seen: set[str] = set()
    for i, start in enumerate(asset_starts):
        end = asset_starts[i + 1] if i + 1 < len(asset_starts) else first_rule
        block = source_text[start:end].rstrip()
        hashes = TILE_HASH.findall(block)
        if len(hashes) != 1 or hashes[0] not in authored or hashes[0] in seen:
            raise ProfileGenerationError("could not identify one unique authored asset block")
        content_id = hashes[0]
        seen.add(content_id)
        additions = generated.get(content_id, {})
        if (tile_roles is not None and not authored[content_id].get('tile_semantics_authored')
                and authored[content_id].get('tile_role') != 'door'):
            block = re.sub(r'(?m)^tile_role\s*=\s*[^\n]*\n?', '', block)
            if content_id in tile_roles:
                block += '\ntile_role = ' + json.dumps(tile_roles[content_id])
        if content_id in (geometry_overrides or set()):
            for name in ("height",):
                block = re.sub(rf"(?m)^{name}\s*=\s*\[[^\]]*\]",
                               _format_layer(name, additions[name]).rstrip(), block)
        output.append(block + "\n")
        for name in LAYERS:
            if name in additions and name not in authored[content_id]:
                output.append(_format_layer(name, additions[name]))
        output.append("\n")
    if seen != authored.keys():
        raise ProfileGenerationError("authored asset blocks did not match parsed profile")
    for content_id in sorted(generated):
        if content_id in authored:
            continue
        output.append(f'[[assets]]\ntile_hash = "{content_id}"\n')
        if tile_roles is not None and content_id in tile_roles:
            output.append('tile_role = ' + json.dumps(tile_roles[content_id]) + '\n')
        for name in LAYERS:
            if name in generated[content_id]:
                output.append(_format_layer(name, generated[content_id][name]))
        output.append("\n")
    if first_rule < len(source_text):
        output.append(source_text[first_rule:].lstrip("\n"))
    rendered = "".join(output).rstrip() + "\n"
    rendered += ''.join(block for _, _, block in group_blocks)
    result = tomllib.loads(rendered)
    validate_groups(result)
    if result.get('obstacle_groups') != parsed_source.get('obstacle_groups'):
        raise ProfileGenerationError('obstacle group payload changed')
    return rendered


def _complete_hud_group(source_text: str, profile: dict,
                        connection: sqlite3.Connection) -> tuple[str, int]:
    """Include all ROM HUD art; the existing rule limits it to background 3."""
    group = profile.get("asset_groups", {}).get("hud_tiles")
    if group is None:
        return source_text, 0
    existing = set(group["tile_hashes"])
    rom_hud = {row[0] for row in connection.execute(
        "SELECT DISTINCT content_id FROM graphics_source "
        "WHERE source_kind='background' AND pack_index IN (113, 114) "
        "AND content_id LIKE 'v1:2bpp:%'")}
    if not existing <= rom_hud:
        raise ProfileGenerationError("authored HUD group has hashes outside ROM HUD packs")
    conflicts = {item for name, other in profile.get("asset_groups", {}).items()
                 if name != "hud_tiles" for item in other["tile_hashes"] if item in rom_hud}
    if conflicts:
        raise ProfileGenerationError("ROM HUD hashes overlap another asset group")
    if not any(rule.get("asset_group") == "hud_tiles" and
               rule.get("source") == "background" and rule.get("source_index") == 2
               for rule in profile.get("rules", [])):
        raise ProfileGenerationError("HUD group needs its background-3-only rule")
    line = "tile_hashes = [" + ", ".join(json.dumps(item) for item in sorted(rom_hud)) + "]"
    updated, count = HUD_GROUP.subn(lambda match: match.group(1) + line, source_text)
    if count != 1:
        raise ProfileGenerationError("could not locate exactly one HUD group")
    return updated, len(rom_hud - existing)


def _complete_semantic_groups(source_text: str, profile: dict,
                               semantics: dict[str, dict]) -> tuple[str, dict[str, int]]:
    additions = {}
    obstacle_ids = {content_id for content_id, annotation in semantics.items()
                    if annotation["role"] == "obstacle" and not annotation["context_dependent"]}
    target_groups = {"floor": "dungeon_floor", "wall": "wall_faces",
                     "wall_corner": "wall_faces", "obstacle": "obstacles"}
    assigned = {content_id: target_groups[annotation["role"]]
                for content_id, annotation in semantics.items()
                if annotation["role"] in target_groups and not annotation["context_dependent"] and
                annotation.get("corner_type") != "unknown"}
    # Profile groups are exclusive. Explicit obstacle annotations replace a
    # legacy material classification without changing the tile's authored relief.
    for name, group in profile.get("asset_groups", {}).items():
        removed = {content_id for content_id in group["tile_hashes"]
                   if content_id in assigned and assigned[content_id] != name}
        if not removed:
            continue
        remaining = set(group["tile_hashes"]) - removed
        pattern = re.compile(rf'(?ms)^\[asset_groups\.{re.escape(name)}\]\n.*?(?=^\[|\Z)')
        if remaining:
            def replace_group(match):
                line = "tile_hashes = [" + ", ".join(json.dumps(item) for item in sorted(remaining)) + "]"
                return re.sub(r'tile_hashes\s*=\s*\[[^\n]*\]', lambda _: line, match.group())
            source_text = pattern.sub(replace_group, source_text, count=1)
        else:
            if any(rule.get("asset_group") == name for rule in profile.get("rules", [])):
                raise ProfileGenerationError(f"obstacle annotations empty referenced group {name}")
            source_text = pattern.sub("", source_text, count=1)
        additions[name] = -len(removed)
    roles = {"dungeon_floor": {"floor"},
             "wall_faces": {"wall", "wall_corner"},
             "obstacles": {"obstacle"}}
    for group_name, accepted in roles.items():
        group = profile.get("asset_groups", {}).get(group_name)
        if group is None and group_name == "obstacles":
            semantic_ids = {content_id for content_id, annotation in semantics.items()
                            if annotation["role"] == "obstacle" and
                            not annotation["context_dependent"]}
            if semantic_ids:
                section = ('[asset_groups.obstacles]\n'
                           'tile_hashes = [' + ', '.join(json.dumps(item) for item in sorted(semantic_ids)) + ']\n\n')
                first_asset = source_text.find("[[assets]]")
                if first_asset < 0:
                    first_asset = len(source_text)
                source_text = source_text[:first_asset] + section + source_text[first_asset:]
                additions[group_name] = len(semantic_ids)
            continue
        if group is None:
            raise ProfileGenerationError(f"profile lacks {group_name} asset group")
        existing = set(group["tile_hashes"])
        existing = {content_id for content_id in existing
                    if content_id not in assigned or assigned[content_id] == group_name}
        semantic_ids = {content_id for content_id, annotation in semantics.items()
                        if annotation["role"] in accepted and
                        not annotation["context_dependent"] and
                        annotation.get("corner_type") != "unknown"}
        combined = existing | semantic_ids
        pattern = re.compile(rf'(?ms)^(\[asset_groups\.{re.escape(group_name)}\]\n)'
                             r'tile_hashes\s*=\s*\[[^\n]*\]')
        line = "tile_hashes = [" + ", ".join(json.dumps(item) for item in sorted(combined)) + "]"
        source_text, count = pattern.subn(lambda match: match.group(1) + line,
                                          source_text, count=1)
        if count != 1:
            raise ProfileGenerationError(f"could not locate {group_name} asset group")
        additions[group_name] = len(combined - existing)
    return source_text, additions


def _enable_upper_floor_height(source_text: str, profile: dict) -> tuple[str, int]:
    existing = profile.get("lighting_space", {}).get("upper_floor_height")
    if existing is not None:
        return source_text, existing
    height = 50  # Leave room above the tallest authored wall (47).
    updated, count = re.subn(r"(?m)^schema_version\s*=\s*\d+\s*$",
                             f"schema_version = {max(13, profile.get('schema_version', 1))}", source_text, count=1)
    if count != 1:
        raise ProfileGenerationError("could not update candidate schema")
    updated, count = re.subn(r"(?m)^\[lighting_space\]\s*$",
                             f"[lighting_space]\nupper_floor_height = {height}", updated, count=1)
    if count != 1:
        raise ProfileGenerationError("candidate has no lighting_space section")
    return updated, height


def generate_profile(database: Path, source: Path, output: Path,
                     semantics_path: Path | None = None,
                     reference_templates_path: Path | None = None,
                     preserve_authored_geometry: bool = False) -> dict:
    if output.resolve() in {database.resolve(), source.resolve()}:
        raise ProfileGenerationError("candidate profile must be distinct from inputs")
    source_snapshot = source.read_bytes()
    source_text = source_snapshot.decode()
    profile = tomllib.loads(source_text)
    try:
        _, obstacle_ids = validate_groups(profile)
    except ValueError as e:
        raise ProfileGenerationError(str(e)) from e
    authored = {asset["tile_hash"]: asset for asset in profile.get("assets", [])}
    declared_floor_ids = set(profile.get("asset_groups", {}).get(
        "dungeon_floor", {}).get("tile_hashes", []))
    if len(authored) != len(profile.get("assets", [])):
        raise ProfileGenerationError("input profile has duplicate asset hashes")
    connection = sqlite3.connect(f"file:{database}?mode=ro", uri=True)
    try:
        run = dict(connection.execute("SELECT key, value FROM run_info"))
        if run.get("rom_sha256") != profile.get("game", {}).get("rom_sha256"):
            raise ProfileGenerationError("atlas ROM hash differs from profile")
        semantics = {}
        templates = (ReferenceTemplates(reference_templates_path, authored, run["rom_sha256"])
                     if reference_templates_path else None)
        reference_records = []
        if semantics_path:
            authored_semantics = AnnotationStore(semantics_path, run["rom_sha256"]).tiles
            catalog = TileCatalog(database)
            # Explicit native/profile annotations take precedence over stale HTML sidecars.
            for key, asset in authored.items():
                if asset.get('tile_semantics_authored') or asset.get('tile_role') == 'door':
                    annotation = {'role': asset.get('tile_role', 'unknown'),
                                  'normals': asset.get('tile_semantic_normals', []),
                                  'context_dependent': asset.get('tile_semantic_context_dependent', False),
                                  'note': asset.get('tile_semantic_note', '')}
                    if asset.get('tile_semantic_corner'):
                        annotation['corner_type'] = asset['tile_semantic_corner']
                    if 'doorframe_mask' in asset:
                        annotation['doorframe_mask'] = asset['doorframe_mask']
                    annotation = validate_annotation(annotation)
                    for alias in catalog.semantic_group(key):
                        authored_semantics[alias] = annotation
            if templates:
                for entry in templates.entries:
                    if 'tile_hashes' in entry:
                        excluded = set(entry.get('tile_hashes', [])) | {entry['reference']}
                        entry['tile_hashes'] = sorted({alias for key in excluded
                                                      for alias in catalog.semantic_group(key)})
            for content_id, annotation in authored_semantics.items():
                for alias in catalog.semantic_group(content_id):
                    existing = semantics.get(alias)
                    if existing is not None and existing != annotation:
                        raise ProfileGenerationError(
                            f"conflicting exact-source semantics for {content_id} and {alias}")
                    semantics[alias] = annotation
        rows = list(connection.execute(
            "SELECT d.content_id, d.indices, GROUP_CONCAT(DISTINCT s.source_kind) "
            "FROM decoded_tile d JOIN graphics_source s USING(content_id) "
            "GROUP BY d.content_id ORDER BY d.content_id"))
        source_text, new_hud_ids = _complete_hud_group(source_text, profile, connection)
        deferred_semantics = {}
        if preserve_authored_geometry:
            # Activation adds missing semantics; it must not erase already
            # authored structural usage contracts. Report disagreements rather
            # than silently replacing a wall/stair with a global floor label.
            source_roles = {key: a['tile_role'] for key, a in authored.items()
                            if a.get('tile_role') not in (None, 'unknown', 'other')}
            for name, role in {'wall_faces': 'wall', 'wall_tops': 'floor',
                               'dungeon_floor': 'floor', 'obstacles': 'obstacle',
                               'stair_treads': 'stair_top', 'stair_rails': 'stair_top'}.items():
                for key in profile.get('asset_groups', {}).get(name, {}).get('tile_hashes', []):
                    source_roles.setdefault(key, role)
            for key, role in source_roles.items():
                annotation = semantics.get(key)
                if annotation and annotation['role'] != role and not (
                        role in ('wall', 'wall_corner') and annotation['role'] in ('wall', 'wall_corner')):
                    deferred_semantics[key] = {'source_role': role, 'annotation_role': annotation['role']}
                    semantics.pop(key)
            for key, annotation in list(semantics.items()):
                a = authored.get(key, {})
                if (key not in source_roles and annotation['role'] in ('wall', 'wall_corner') and
                        any(a.get('height', []))):
                    # Existing nonzero unclassified relief can be an absolute
                    # multi-tile template member, not a globally local wall.
                    # Preserve its assembly-only usage until reauthoring or
                    # explicit placement ownership resolves the disagreement.
                    deferred_semantics[key] = {'source_role': 'assembly_only_authored_relief',
                                               'annotation_role': annotation['role']}
                    semantics.pop(key)
        source_text, semantic_group_additions = _complete_semantic_groups(
            source_text, profile, semantics) if semantics else (source_text, {})
        source_text, upper_floor_height = _enable_upper_floor_height(source_text, profile)
        references = _link_references(connection, authored)
        generated: dict[str, dict[str, tuple[int, ...]]] = {}
        methods: Counter[str] = Counter()
        ambiguous: list[str] = []
        authored_layer_counts = Counter()
        geometry_overrides: set[str] = set()
        for content_id, indices, families_text in rows:
            families = set(families_text.split(","))
            if len(indices) != 64:
                raise ProfileGenerationError(f"atlas tile {content_id} is not 8x8")
            if len(families) > 1:
                ambiguous.append(content_id)
            if content_id in authored:
                method = "authored_preserved"
                proposal = (_silhouette_layers(indices) if "link" in families or "sprite" in families
                            else _neutral_layers())
            elif families == {"background"} and (
                    content_id in STAIR_TREAD_HEIGHT or content_id in STAIR_RAIL_SEGMENT):
                method = ("stair_tread" if content_id in STAIR_TREAD_HEIGHT else "stair_rail")
                proposal = _stair_layers(content_id, indices)
            elif families == {"background"} and content_id in UPPER_GUARD_RAIL_TILES:
                method = "upper_guard_rail"
                proposal = _upper_guard_rail_layers(indices)
            elif families == {"background"} and content_id in WALL_OVERLAY_SEGMENTS:
                method = "wall_overlay"
                proposal = _wall_overlay_layers(content_id)
            elif (families == {"background"} and content_id in semantics and
                  semantics[content_id]["role"] in ("wall", "wall_corner") and
                  not semantics[content_id]["context_dependent"] and
                  semantics[content_id].get("corner_type") != "unknown"):
                method = "semantic_" + semantics[content_id]["role"]
                proposal = (_neutral_layers() if templates else
                            _semantic_wall_layers(semantics[content_id]))
            elif (families == {"background"} and content_id in semantics and
                  semantics[content_id]["role"] == "floor" and
                  not semantics[content_id]["context_dependent"]):
                method = "semantic_floor"
                proposal = _neutral_layers()
            elif len(families) > 1:
                method = "shared_family_neutral"
                proposal = _neutral_layers()
            elif families == {"link"}:
                method = "link_silhouette"
                proposal = _silhouette_layers(indices)
                transferred = _transfer_link(indices, references, proposal)
                if transferred is not None:
                    method = "link_authored_reference_transfer"
                    proposal = transferred
            elif families == {"sprite"}:
                method = "sprite_silhouette"
                proposal = _silhouette_layers(indices)
            else:
                method = "background_neutral"
                proposal = _neutral_layers()
            methods[method] += 1
            existing = authored.get(content_id, {})
            for name in LAYERS:
                if name in existing:
                    authored_layer_counts[name] += 1
            if "height" in existing and "normal_xyz" not in existing and not templates:
                proposal["normal_xyz"] = _normal_from_height(existing["height"])
                methods["normal_from_authored_height"] += 1
            semantic_floor = (content_id in semantics and semantics[content_id]["role"] == "floor" and
                              not semantics[content_id]["context_dependent"])
            semantic_obstacle = (content_id in semantics and semantics[content_id]["role"] == "obstacle" and
                                  not semantics[content_id]["context_dependent"])
            semantic_door = existing.get('tile_role') == 'door' or semantics.get(content_id, {}).get('role') == 'door'
            if semantic_floor or (content_id in declared_floor_ids and not semantic_obstacle and not semantic_door):
                proposal["height"] = (0,) * 64
                proposal["normal_xyz"] = FRONT * 64
                methods["semantic_floor_override"] += 1
                geometry_overrides.add(content_id)
            annotation = semantics.get(content_id)
            if (not semantic_floor and annotation and annotation["role"] in ("wall", "wall_corner") and
                    not annotation["context_dependent"] and
                    annotation.get("corner_type") != "unknown"):
                if templates:
                    local, record = templates.resolve(content_id, annotation)
                    reference_records.append(record)
                else:
                    local = _semantic_wall_layers(annotation)
                if local:
                    proposal.update(local)
                    geometry_overrides.add(content_id)
                    methods["reference_wall_override" if templates else "semantic_wall_override"] += 1
            elif not semantic_floor and not semantic_door and templates and any(content_id == e['reference'] or content_id in e.get("tile_hashes", [])
                                   for e in templates.entries):
                local, record = templates.resolve(content_id, annotation or {})
                reference_records.append(record)
                if local:
                    proposal.update(local)
                    geometry_overrides.add(content_id)
            # Authored lighting is an absolute invariant, including legacy
            # generated arrays. No geometry path may regenerate it.
            if "normal_xyz" in existing:
                proposal["normal_xyz"] = tuple(existing["normal_xyz"])
            if (preserve_authored_geometry or existing.get('tile_role') == 'door' or
                    (annotation and annotation['role'] == 'door')) and "height" in existing:
                proposal["height"] = tuple(existing["height"])
                geometry_overrides.discard(content_id)
            if content_id in obstacle_ids:
                # Group authoring is not permission for generated local metadata
                # or intrinsic role changes. Preserve absent sources as absent.
                proposal = {name: tuple(existing[name]) for name in LAYERS if name in existing}
                geometry_overrides.discard(content_id)
                if content_id not in authored:
                    continue # no fabricated hash-only asset table for missing source
            generated[content_id] = proposal
    finally:
        connection.close()
    if not authored.keys() <= generated.keys():
        raise ProfileGenerationError("profile contains assets absent from ROM atlas")
    tile_roles = {content_id: annotation["role"] for content_id, annotation in semantics.items()
                  if (not annotation["context_dependent"] or annotation['role'] == 'door') and annotation.get("corner_type") != "unknown"}
    if preserve_authored_geometry:
        tile_roles = {**tile_roles, **{key: a['tile_role'] for key, a in authored.items()
                                      if a.get('tile_role') not in (None, 'unknown')}}
    for key in obstacle_ids:
        tile_roles.pop(key, None)
        if key in authored and 'tile_role' in authored[key]:
            tile_roles[key] = authored[key]['tile_role']
    candidate = _render_profile(source_text, authored, generated, geometry_overrides,
                                tile_roles if semantics_path else None)
    # Door masks are metadata, never a request to flatten an opening. Carry
    # sidecar-only masks as well as native/profile fields into the candidate.
    door_annotations = {key: value for key, value in semantics.items() if value['role'] == 'door'}
    def add_door_mask(match):
        block = match.group()
        keys = TILE_HASH.findall(block)
        annotation = door_annotations.get(keys[0]) if len(keys) == 1 else None
        if annotation and 'doorframe_mask' in annotation and 'doorframe_mask' not in block:
            block = block.rstrip() + '\ndoorframe_mask = ' + json.dumps(annotation['doorframe_mask']) + '\n\n'
        return block
    candidate = re.sub(r'(?ms)^\[\[assets\]\]\s*\n.*?(?=^\[|\Z)', add_door_mask, candidate)
    if semantics_path:
        # Carry canonical diagonal role/facing separately from lighting arrays.
        # Deferred intrinsic relief does not mean its approved semantic facing
        # may be dropped: runtime extent requires this explicit classification.
        from alttp_activate_diagonal_semantics import activate
        candidate, _ = activate(candidate, json.loads(semantics_path.read_text()))
    signed_schema = 27 if any(v < 0 or v > 255 for a in tomllib.loads(candidate).get('assets', []) for v in a.get('height', ())) else 1
    semantic_schema = max(signed_schema, 21 if door_annotations else 18)
    if semantics_path:
        candidate = re.sub(r'(?m)^schema_version\s*=\s*\d+\s*$',
                            f"schema_version = {max(semantic_schema, profile.get('schema_version', 1))}", candidate, count=1)
    parsed = tomllib.loads(candidate)
    validate_groups(parsed)
    for key, value in profile.items():
        if key == "schema_version":
            value = max(signed_schema, value)
        if key == "schema_version" and semantics_path:
            value = max(semantic_schema, value)
        if key == "schema_version" and upper_floor_height and value < 13:
            value = 13
        if key == "lighting_space" and upper_floor_height:
            value = {**value, "upper_floor_height": upper_floor_height}
        if key == "asset_groups" and new_hud_ids:
            expected = {name: dict(group) for name, group in value.items()}
            expected["hud_tiles"]["tile_hashes"] = parsed[key]["hud_tiles"]["tile_hashes"]
            for name in semantic_group_additions:
                if name in parsed[key]:
                    expected[name] = parsed[key][name]
                else:
                    expected.pop(name, None)
            value = expected
        elif key == "asset_groups" and semantic_group_additions:
            expected = {name: dict(group) for name, group in value.items()}
            for name in semantic_group_additions:
                if name in parsed[key]:
                    expected[name] = parsed[key][name]
                else:
                    expected.pop(name, None)
            value = expected
        if key != "assets" and parsed.get(key) != value:
            raise ProfileGenerationError(f"non-asset profile section changed: {key}")
    candidate_assets = parsed.get("assets", [])
    if len(candidate_assets) != len(generated):
        raise ProfileGenerationError("candidate asset count differs from ROM atlas")
    if len({asset["tile_hash"] for asset in candidate_assets}) != len(candidate_assets):
        raise ProfileGenerationError("candidate has duplicate asset hashes")
    for asset in candidate_assets:
        if asset['tile_hash'] not in obstacle_ids and any(name not in asset for name in LAYERS):
            raise ProfileGenerationError(f"candidate {asset['tile_hash']} lacks a layer")
        original = authored.get(asset["tile_hash"])
        allowed_floor_override = asset["tile_hash"] in geometry_overrides
        if original and any(asset.get(name) != value for name, value in original.items()
                            if name != "tile_role" and (not allowed_floor_override or name != "height")):
            raise ProfileGenerationError(f"authored asset changed: {asset['tile_hash']}")
    output.parent.mkdir(parents=True, exist_ok=True)
    if source.read_bytes() != source_snapshot:
        raise ProfileGenerationError("source changed during generation; candidate not written")
    temporary = output.with_name(output.name + ".tmp")
    try:
        temporary.write_text(candidate)
        os.replace(temporary, output)
    finally:
        if temporary.exists():
            temporary.unlink()
    return {"candidate_profile": str(output), "candidate_bytes": len(candidate.encode()),
             "source_sha256": hashlib.sha256(source_snapshot).hexdigest(),
             "candidate_sha256": hashlib.sha256(candidate.encode()).hexdigest(),
             "normal_arrays_changed": 0,
             "height_arrays_changed": sum(a.get('height') != authored[a['tile_hash']].get('height')
                                          for a in candidate_assets if a['tile_hash'] in authored),
             "raw_semantic_annotations": len(authored_semantics) if semantics_path else 0,
             "expanded_semantic_annotations": len(semantics) + len(deferred_semantics),
             "preserve_authored_geometry": preserve_authored_geometry,
             "deferred_semantic_role_conflicts": deferred_semantics,
            "assets": len(generated), "new_assets": len(generated) - len(authored),
            "authored_assets_preserved": len(authored),
            "authored_layer_counts": dict(authored_layer_counts),
            "generated_methods": dict(methods), "ambiguous_shared_family_ids": ambiguous,
             "generated_layer_coverage": {name: sum(name in a for a in candidate_assets) for name in LAYERS},
             "obstacle_groups_preserved": len(profile.get('obstacle_groups', [])),
             "obstacle_missing_local_sources": sorted(h for h in obstacle_ids if 'height' not in authored.get(h, {})),
            "new_hud_hashes": new_hud_ids,
             "upper_floor_height": upper_floor_height,
             "semantic_annotations": len(semantics),
             "semantic_group_additions": semantic_group_additions,
             "local_geometry_overrides": len(geometry_overrides),
              "reference_templates": templates.report(reference_records) if templates else None,
             "confidence": "first-pass visual heuristics; review in game"}


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--atlas", type=Path, required=True)
    parser.add_argument("--source", type=Path, default=Path("remaster/profiles/ALttP/alttp-profile.toml"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--tile-semantics", type=Path,
                        default=Path("remaster/tools/ALttP/alttp-tile-semantics-v1.json"))
    parser.add_argument("--update-source", action="store_true",
                        help="promote the candidate to --source, keeping a unique byte-exact backup")
    parser.add_argument("--reference-templates", type=Path,
                        help="opt-in reference-derived local wall geometry; never replaces existing normals")
    parser.add_argument("--report", type=Path, help="write reproducible generation/evidence report")
    parser.add_argument("--preserve-authored-geometry", action="store_true",
                        help="activate semantic roles without replacing existing height arrays (including zero arrays)")
    args = parser.parse_args()
    try:
        if args.report and args.report.resolve() in {p.resolve() for p in
                (args.source, args.output, args.atlas, args.tile_semantics,
                 *([args.reference_templates] if args.reference_templates else []))}:
            raise ProfileGenerationError("report must be distinct from all inputs and candidate")
        source_snapshot = args.source.read_bytes() if args.update_source else None
        report = generate_profile(args.atlas, args.source, args.output, args.tile_semantics,
                                  args.reference_templates, args.preserve_authored_geometry)
        if args.report:
            args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
        if args.update_source:
            if args.source.read_bytes() != source_snapshot:
                raise ProfileGenerationError("source changed during generation; candidate was not promoted")
            index = 0
            while True:
                backup = args.source.with_name(args.source.stem + ".before-heights" +
                    (f"_{index:03d}" if index else "") + args.source.suffix)
                try:
                    with backup.open("xb") as saved:
                        saved.write(source_snapshot)
                    break
                except FileExistsError:
                    index += 1
            if args.source.read_bytes() != source_snapshot:
                raise ProfileGenerationError("source changed before promotion; backup and candidate retained")
            # Candidate and source are on the same workspace filesystem here.
            # Replacement is atomic; existing files are never partially rewritten.
            os.replace(args.output, args.source)
            report["updated_source"] = str(args.source)
            report["backup"] = str(backup)
    except (ProfileGenerationError, ValueError, OSError) as exc:
        parser.exit(1, f"{exc}\n")
    print(json.dumps({key: report[key] for key in (
        "candidate_profile", "assets", "semantic_annotations",
        "semantic_group_additions", "local_geometry_overrides")}, indent=2))
    if args.update_source:
        print(json.dumps({key: report[key] for key in ("updated_source", "backup")}, indent=2))
