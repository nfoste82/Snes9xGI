#!/usr/bin/env python3
"""Build and serve a ROM-backed, assembled NPC pose authoring workbench."""
from __future__ import annotations

import argparse
from collections import defaultdict
import hashlib
from http.server import BaseHTTPRequestHandler
import json
import math
import os
from pathlib import Path
import re
import sqlite3
import subprocess
import tempfile
import tomllib
import webbrowser

from alttp_atlas import SUPPORTED_US_SHA256
from alttp_atlas_graphics import tile_content_id
from alttp_npc_workbench_support import sprite_tilesets, special_overworld_contexts
from alttp_npc_catalog import CATALOG, MODELED_TYPES
from alttp_npc_coverage import link_sequences, build_coverage, compare_observations

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUTPUT = ROOT / "build/remaster-geometry/npc-workbench"


def atomic_write(path: Path, text: str):
    with tempfile.NamedTemporaryFile(mode="w", dir=path.parent, delete=False) as file:
        tmp = Path(file.name)
        file.write(text)
    try:
        os.replace(tmp, path)
    finally:
        tmp.unlink(missing_ok=True)


def build_adapter(engine: Path, output: Path) -> Path:
    if hashlib.sha256((engine / "zelda3.sfc").read_bytes()).hexdigest() != SUPPORTED_US_SHA256:
        raise ValueError("reference engine ROM does not match the supported US ROM")
    flags = subprocess.check_output(["sdl2-config", "--cflags"], text=True).split()
    libs = subprocess.check_output(["sdl2-config", "--libs"], text=True).split()
    obj = output / "npc_adapter.o"
    subprocess.run(["clang", "-O2", "-std=gnu11", "-I", str(engine), *flags,
                    "-DSYSTEM_VOLUME_MIXER_AVAILABLE=0", "-c",
                    str(Path(__file__).with_name("alttp_npc_pose_adapter.c")),
                    "-o", str(obj)], check=True)
    objects = [p for p in (engine / "src").glob("*.o") if p.name != "main.o"]
    objects += list((engine / "snes").glob("*.o"))
    objects += [engine / "third_party/gl_core/gl_core_3_1.o",
                engine / "third_party/opus-1.3.1-stripped/opus_decoder_amalgam.o", obj]
    executable = output / "alttp-npc-poses"
    subprocess.run(["clang", *map(str, objects), *libs, "-lm", "-o", str(executable)], check=True)
    return executable


def contexts(atlas: Path, model=None, special=None):
    result = defaultdict(set)
    with sqlite3.connect(f"file:{atlas}?mode=ro", uri=True) as db:
        rooms = {r["room_id"]: r for (text,) in db.execute(
            "SELECT data_json FROM world_record WHERE kind='dungeon_rooms'")
                 for r in [json.loads(text)]}
        for kind in ("overworld_sprites", "dungeon_sprites"):
            for (text,) in db.execute("SELECT data_json FROM world_record WHERE kind=?", (kind,)):
                r = json.loads(text)
                expected_type = model.get("context_type", model["type_id"]) if model else None
                expected_kind = model.get("context_kind", "sprite") if model else "sprite"
                if (r["type_id"] != expected_type if model else r["type_id"] not in MODELED_TYPES) or r.get("record_kind", "sprite") != expected_kind:
                    continue
                if model:
                    if r["type_id"] != model.get("context_type", model["type_id"]):
                        continue
                    room_id = r.get("room_id")
                    if "rooms" in model and room_id not in model["rooms"]:
                        continue
                    if room_id in model.get("exclude_rooms", []):
                        continue
                if kind == "overworld_sprites":
                    world = int(0x40 <= r["area_id"] < 0x80)
                    if model and "world" in model and world != model["world"]:
                        continue
                    gfx, palette = r.get("gfx_index"), r.get("palette_index")
                    if gfx is None or palette is None:
                        gfx, palette = (special or {}).get(r["area_id"], (None, None))
                    if gfx is None or palette is None:
                        continue
                    result[r["type_id"]].add((gfx, palette,
                                             world, f"overworld {r['area_id']:03x}, {r['stage']}"))
                else:
                    room = rooms[r["room_id"]]
                    # Dungeon palette selector is distinct from the OW selector;
                    # retain it as an explicitly unverified palette context.
                    world = model.get("world", int(r["room_id"] in [0x10f,0x110,0x11f,0x11c])) if model else 0
                    dungeon_palette = bool(model and model.get("dungeon_palette"))
                    palette = 20 + room["palette"] if dungeon_palette else 0
                    note = "room-header dungeon palette" if dungeon_palette else "palette provisional"
                    result[model["type_id"] if model else r["type_id"]].add((room["enemy_blockset"] + 0x40, palette, world,
                                              f"room {r['room_id']:03x} ({note})"))
    return result


def prepare_pose(pose: dict):
    parts = pose["parts"]
    # OAM slot order is front-to-back. Remove empty tiles, not hidden body art.
    parts[:] = [p for p in parts if any(p["indices"])]
    for p in parts:
        p["tile_hash"] = tile_content_id(bytes(p["indices"]), 4)
    if not parts:
        raise ValueError("empty NPC pose")
    x0, y0 = min(p["x"] for p in parts), min(p["y"] for p in parts)
    x1, y1 = max(p["x"] + 8 for p in parts), max(p["y"] + 8 for p in parts)
    pose["bounds"] = [x0, y0, x1, y1]
    pose["facing"] = (["east", "west", "south", "north"] if pose.get("guard")
                      else ["north", "south", "west", "east"])[pose["direction"]]
    return pose


def generate(engine: Path, atlas: Path, output: Path):
    output.mkdir(parents=True, exist_ok=True)
    executable = build_adapter(engine, output)
    tilesets = sprite_tilesets(engine)
    special = special_overworld_contexts(engine)
    actors = []
    with sqlite3.connect(f"file:{atlas}?mode=ro", uri=True) as db:
        bg_hashes = {r[0] for r in db.execute("SELECT DISTINCT content_id FROM graphics_source WHERE source_kind='background'")}
    missing_models = []
    player_sequences = link_sequences(engine)
    for model in CATALOG:
        type_id, name, action = model["type_id"], model["name"], model["action"]
        indexed = defaultdict(set) if model.get("explicit_contexts") else contexts(atlas, model, special)
        grouped = defaultdict(list)
        for gfx, palette, world, witness in sorted(indexed[type_id]):
            grouped[gfx, palette, world].append(witness)
        for gfx, palette, world, witness in model.get("explicit_contexts", []):
            grouped[gfx, palette, world].append(witness)
        if not grouped:
            missing_models.append(dict(name=name, reason="No matching placed context; dynamic/cutscene state needs a context adapter."))
        for (gfx, palette, world), witnesses in grouped.items():
            if model.get("context_note"):
                witnesses = [model["context_note"], *witnesses]
            if not 0 <= palette < 256:
                continue
            sequences = player_sequences if model["draw"] == "link" else [dict(action=action, steps=model['steps'])]
            raw = {"poses": []}
            # Isolate equipment/palette variants, but retain actions together so
            # the native browser can filter and author all catalogued actions.
            equipment = [(0, 0, 0)] if model['draw'] != 'link' else [
                (0, 0, 0), (1, 1, 0), (2, 2, 0), (3, 3, 0), (4, 3, 0),
                (2, 2, 1), (2, 2, 2)]
            for sword, shield, armor in equipment:
                variant_poses = []
                for sequence in sequences:
                    state = sequence.get('adapter_state', 0)
                    if model['draw'] == 'link' and not sword and state in (1, 2, 3, 4, 11):
                        continue
                    if model['draw'] == 'link' and sword and state == 5:
                        continue  # Bunny has no equipment; do not invent equipped bunnies.
                    command = [str(executable), str(type_id), str(gfx), str(palette), model["draw"],
                               str(model["directions"]), str(sequence["steps"]), str(world),
                               str(model["follower"]), str(model["variant"])]
                    if model['draw'] == 'link':
                        command += list(map(str, (state, sword, shield, armor)))
                    run = subprocess.run(command, cwd=engine, capture_output=True, text=True, check=True, timeout=30)
                    drawn = json.loads(run.stdout)['poses']
                    if len(drawn) != model['directions'] * sequence['steps']:
                        raise ValueError(f"Incomplete draw sequence: {name}, {sequence['action']}")
                    for pose in drawn:
                        pose['action'] = sequence['action']
                        pose['evidence'] = sequence.get('evidence', 'Legacy bounded adapter; bounds need source review.')
                        if model['draw'] == 'link' and state in (2, 4):
                            pose['duration_frames'] = 1
                        if model['draw'] == 'link':
                            expected_action = [0, 39, 2, 3, 15, 33, 17, 5, 20, 29, 30, 36][state]
                            if state == 4 and pose['draw_state']['handler'] == 0:
                                expected_action = 0  # Body rests while the sparkle tail finishes.
                            if pose['draw_state']['oam_action'] != expected_action:
                                raise ValueError(f"Link draw-state branch mismatch: {sequence['action']}")
                    variant_poses.extend(drawn)
                raw['poses'] = variant_poses
                if model['draw'] == 'link':
                    actors.append(dict(id=f"link-equipment-{sword}-{shield}-{armor}", name=name,
                                       type_id=type_id, gfx_index=gfx, palette_index=palette, draw_model=model,
                                       world=world, contexts=[f"Sword {sword}, shield {shield}, tunic {armor}; gloves 0"],
                                       poses=variant_poses, graphics_inherited=False,
                                       equipment=dict(sword=sword, shield=shield, armor=armor)))
            # Non-Link actors have one variant here. Link variants were added above.
            generated_actors = actors[-len(equipment):] if model['draw'] == 'link' else [dict(
                id=f"{model['key']}-{gfx:02x}-{palette:02x}-{world}", name=name, type_id=type_id,
                gfx_index=gfx, palette_index=palette, draw_model=model, world=world, contexts=witnesses,
                poses=raw['poses'], graphics_inherited=0 in tilesets[gfx])]
            for actor in generated_actors:
                actor['coverage'] = dict(status='partial', exhaustive=False,
                    gaps=(["Item-use, carrying/lifting, pits, stairs, swimming strokes, medallions, sleep/death and cutscenes remain unmodeled.",
                           "Equipment combinations and gloves are sampled, not exhaustive. Charge and spin-release effects are included; other independent effects remain unmodeled.",
                           "Draw-state vectors need gameplay assembly/palette parity verification."] if model['draw'] == 'link' else
                          ["Legacy bounded frames: attack/recoil/death branches, independent facing/head states and dynamic contexts require source review."]))
            if model['draw'] != 'link':
                actors.extend(generated_actors)
            for actor in generated_actors:
                for pose in actor['poses']:
                    pose["guard"] = 0x3f <= type_id <= 0x4b or model["draw"] == "thief"
                    try:
                        prepare_pose(pose)
                    except ValueError as error:
                        raise ValueError(f"{name}, gfx {gfx:02x}, palette {palette}, direction {pose['direction']}, step {pose['step']}: {error}") from error
                    if model["directions"] == 1:
                        pose["facing"] = "fixed"
                    elif model["draw"] in ("smithy", "bully"):
                        pose["facing"] = ["west", "east"][pose["direction"]]
                    elif model["draw"] == "uncle":
                        pose["facing"] = "fixed"
                        pose["action"] = ["idle", "sword held", "sword extended", "fallen"][pose["direction"]]
                    elif model["draw"] == "ganon":
                        pose["facing"] = ["west", "east"][pose["direction"]]
                    elif model.get("dungeon_palette"):
                        pose["facing"] = ["east", "west", "south", "north"][pose["direction"]]
                    if model["draw"] == "armos-knight":
                        pose["action"] = "blue knight" if pose["step"] < 2 else "red crusher"
                    pose["status"] = "modeled_unverified"
            if model["draw"] == "ganon":
                actors[-1]["contexts"] = ["room 000 (room-header dungeon palette)"]
    actors.sort(key=lambda actor: (actor["name"].casefold(), actor["id"]))
    manifest = {"format": "alttp-npc-workbench-v1", "rom_sha256": SUPPORTED_US_SHA256,
                "actors": actors, "background_shared_hashes": sorted(bg_hashes &
                    {p["tile_hash"] for a in actors for pose in a["poses"] for p in pose["parts"]}),
                 "reference_draw_source_sha256": hashlib.sha256((engine / "src/sprite_main.c").read_bytes()).hexdigest(),
                  "reference_graphics_source_sha256": hashlib.sha256((engine / "src/load_gfx.c").read_bytes()).hexdigest(),
                  "reference_player_draw_source_sha256": hashlib.sha256((engine / "src/player_oam.c").read_bytes()).hexdigest(),
                  "reference_effect_source_sha256": hashlib.sha256((engine / "src/ancilla.c").read_bytes()).hexdigest(),
                  "pose_adapter_source_sha256": hashlib.sha256(Path(__file__).with_name("alttp_npc_pose_adapter.c").read_bytes()).hexdigest(),
                 "catalog_source_sha256": hashlib.sha256(Path(__file__).with_name("alttp_npc_catalog.py").read_bytes()).hexdigest(),
                "coverage": {"modeled_types": sorted({a["type_id"] for a in actors}),
                             "character_families": len({a["name"] for a in actors}),
                             "missing_models": missing_models,
                             "unmodeled_placed_types": sorted(set(all_placed_types(atlas)) - {a["type_id"] for a in actors}),
                             "gaps": ["Other actor draw routines and action branches are unmodeled.",
                                      "Draw-only poses are not gameplay reachability or Snes9x parity proof.",
                                       "Legacy character dungeon palettes remain provisional; new enemy models use room-header palettes. Zero/inherited graphics slots still need entry-state verification.",
                                       "Trinexx, remaining enemies, boss minions/projectiles and additional phases need bounded draw adapters; segmented boss histories are modeled straight travel, not captured gameplay trajectories.",
                                        "Link has bounded movement/sword/action adapters, not all player actions or equipment combinations. Ganon attack effects, travel duck, Venus, Pikku, Ghostly Proprietor and background-composited Talking Tree need further adapters.",
                                      "Crystal maidens share a visual family; individual rescue palettes and reachability remain unverified.",
                                      "Maple is GBA-only and absent from the supported SNES ROM."]}}
    manifest['coverage']['inventory'] = build_coverage(engine, CATALOG, actors)
    atomic_write(output / "coverage.json", json.dumps(manifest['coverage']['inventory'], indent=2)+'\n')
    # Publish only after every bounded state and source inventory validates.
    atomic_write(output / "poses.json", json.dumps(manifest, separators=(",", ":")))
    validator = output / "alttp-profile-validate"
    subprocess.run(["c++", "-std=c++17", "-O2", str(Path(__file__).with_name("alttp_profile_validate.cpp")),
                    "-o", str(validator)], check=True)
    print(f"Generated {len(actors)} contexts, {sum(len(a['poses']) for a in actors)} poses at {output}")


def all_placed_types(atlas):
    with sqlite3.connect(f"file:{atlas}?mode=ro", uri=True) as db:
        return [r[0] for r in db.execute("SELECT DISTINCT type_id FROM placed_sprite_type WHERE family != 'overlord'")]


def audit_captures(output, captures):
    executable = output / 'alttp-npc-capture-audit'
    subprocess.run(['c++', '-std=c++17', '-O2', str(Path(__file__).with_name('alttp_npc_capture_audit.cpp')),
                    '-o', str(executable)], check=True)
    observed = subprocess.check_output([str(executable), *map(str, captures)], text=True)
    manifest = json.loads((output / 'poses.json').read_text())
    result = compare_observations(manifest, json.loads(observed))
    atomic_write(output / 'capture-audit.json', json.dumps(result, indent=2)+'\n')
    for capture in result['captures']:
        print(f"{capture['capture']}: {capture['observed_tiles']} observed OBJ identities; "
              f"{len(capture['absent_from_library'])} absent from library; "
              f"{len(capture['missing_height'])} missing captured height, {len(capture['missing_normals'])} missing captured normals")


def silhouette_defaults(pose: dict) -> dict:
    """Image-derived rounded silhouette normals; local ramps fit runtime alignment.

    Height deliberately uses the existing 13-y local tile contract. Assembled
    absolute heights cannot safely be stored in a reused canonical tile hash.
    """
    mask = set()
    for part in pose["parts"]:
        for p, ink in enumerate(part["indices"]):
            if ink:
                x, y = p % 8, p // 8
                mask.add((part["x"] + (7-x if part["hflip"] else x),
                          part["y"] + (7-y if part["vflip"] else y)))
    proposals = defaultdict(list)
    for part in pose["parts"]:
        normal = [128, 128, 255] * 64
        height, occlusion = [0] * 64, [0] * 64
        for p, ink in enumerate(part["indices"]):
            if not ink:
                continue
            x = part["x"] + (7-p % 8 if part["hflip"] else p % 8)
            y = part["y"] + (7-p // 8 if part["vflip"] else p // 8)
            # Independent contiguous spans prevent a sword and torso from being
            # interpreted as one wide cylinder; palette brightness is not depth.
            left = right = x
            while (left-1, y) in mask:
                left -= 1
            while (right+1, y) in mask:
                right += 1
            nx = 0.72 * (x - (left+right)/2) / max(1, (right-left)/2)
            ny = -0.25 if (x, y-1) not in mask else 0.25 if (x, y+1) not in mask else 0
            nz = math.sqrt(max(0.1, 1-nx*nx-ny*ny))
            if part["hflip"]:
                nx = -nx
            if part["vflip"]:
                ny = -ny
            normal[p*3:p*3+3] = [round((v+1)*127.5) for v in (nx, ny, nz)]
            height[p], occlusion[p] = 13-p//8, 255
        proposals[part["tile_hash"]].append({"height": height, "normal_xyz": normal,
                                             "occlusion": occlusion})
    # A hash shared by different positions may receive incompatible inferred
    # normals. Return that ambiguity; callers must not silently choose a pose.
    return {key: {"layers": items[0], "conflict": any(i != items[0] for i in items[1:])}
            for key, items in proposals.items()}


def validate_layers(layers):
    if not isinstance(layers, dict) or set(layers) - {"height", "normal_xyz", "occlusion"}:
        raise ValueError("unsupported layer")
    for name, data in layers.items():
        if not isinstance(data, list) or len(data) != (192 if name == "normal_xyz" else 64):
            raise ValueError(f"invalid {name} size")
        lo, hi = (-32768, 32767) if name == 'height' else (0, 255)
        if any(type(v) is not int or not lo <= v <= hi for v in data):
            raise ValueError("height values must be signed int16; other layers must be bytes")
        if name == "normal_xyz" and any(data[p:p+3] == [128, 128, 128] for p in range(0, 192, 3)):
            raise ValueError("normal must be nonzero")


def merge_profile(text: str, edits: dict) -> tuple[str, int]:
    """Fill missing layers only, preserving all existing profile text/options."""
    parsed = tomllib.loads(text)
    if parsed["game"]["rom_sha256"] != SUPPORTED_US_SHA256:
        raise ValueError("profile ROM mismatch")
    authored = {a["tile_hash"]: a for a in parsed.get("assets", [])}
    additions, count = {}, 0
    for key, layers in edits.items():
        validate_layers(layers)
        missing = {name: data for name, data in layers.items() if name not in authored.get(key, {})
                   and not (name == "occlusion" and all(v == 255 for v in data))}
        if missing:
            additions[key] = "".join(f"{name} = {json.dumps(data)}\n" for name, data in missing.items())
            count += len(missing)
    headers = list(re.finditer(r"(?m)^\[.*\][ \t]*$", text))
    for i in reversed(range(len(headers))):
        h = headers[i]
        if h.group() != "[[assets]]":
            continue
        end = headers[i+1].start() if i+1 < len(headers) else len(text)
        record = tomllib.loads(text[h.start():end])["assets"][0]
        key = record["tile_hash"]
        if key in additions:
            text = text[:end].rstrip() + "\n" + additions.pop(key) + "\n" + text[end:]
    for key, block in sorted(additions.items()):
        text += f'\n[[assets]]\ntile_hash = "{key}"\n{block}'
    # New NPC tiles need a surface/material rule as well as pixel maps. Keep
    # existing classifications authoritative and use an OBJ-only fallback group.
    existing_members = {key for group in parsed.get("asset_groups", {}).values()
                        for key in group.get("tile_hashes", [])}
    new_members = sorted(set(edits) - existing_members)
    if count and new_members:
        material = "npc"
        if material not in parsed.get("materials", {}):
            material = "npc_workbench_default"
            if material not in parsed.get("materials", {}):
                text += (f'\n[materials.{material}]\nsurface_class = "prop"\n'
                         'roughness = 0.8\nmetalness = 0\ncasts_shadow = true\n')
        group = "npc_workbench"
        suffix = 1
        while group in parsed.get("asset_groups", {}):
            suffix += 1
            group = f"npc_workbench_{suffix}"
        text += (f'\n[asset_groups.{group}]\ntile_hashes = {json.dumps(new_members)}\n'
                 f'\n[[rules]]\nasset_group = "{group}"\nsource = "object"\nmaterial = "{material}"\n')
    # Independent normal layers require schema v5. Keep newer versions intact.
    required_schema = 27 if any(v < 0 or v > 255 for layers in edits.values() for v in layers.get('height', ())) else 5
    if count and parsed["schema_version"] < required_schema:
        text = re.sub(r"(?m)^schema_version\s*=\s*\d+", f"schema_version = {required_schema}", text, count=1)
    result = tomllib.loads(text)
    for name in parsed:
        if name not in ("assets", "schema_version", "asset_groups", "rules", "materials") and result[name] != parsed[name]:
            raise ValueError(f"profile section changed: {name}")
    for name, material in parsed.get("materials", {}).items():
        if result["materials"][name] != material:
            raise ValueError("existing material changed")
    for name, group in parsed.get("asset_groups", {}).items():
        if result["asset_groups"][name] != group:
            raise ValueError("existing group changed")
    if result.get("rules", [])[:len(parsed.get("rules", []))] != parsed.get("rules", []):
        raise ValueError("existing rules changed")
    return text, count


class Workbench:
    def __init__(self, output: Path, profile: Path | None):
        self.output, self.profile = output, profile
        self.manifest = json.loads((output / "poses.json").read_text())
        self.tiles = {p["tile_hash"] for a in self.manifest["actors"] for pose in a["poses"] for p in pose["parts"]}
        path = output / "edits.json"
        self.edits = json.loads(path.read_text()) if path.exists() else {}
        self.revision = 0

    def state(self):
        profile = tomllib.loads(self.profile.read_text()) if self.profile else {}
        if profile and profile["game"]["rom_sha256"] != SUPPORTED_US_SHA256:
            raise ValueError("profile ROM mismatch")
        layers = {a["tile_hash"]: {k: a[k] for k in ("height", "normal_xyz", "occlusion") if k in a}
                  for a in profile.get("assets", []) if a["tile_hash"] in self.tiles}
        return {"manifest": self.manifest, "edits": self.edits, "authored": layers,
                "revision": self.revision, "profile": str(self.profile) if self.profile else None}


def serve(output: Path, profile: Path | None, port: int, launch: bool):
    workbench = Workbench(output, profile)

    class Handler(BaseHTTPRequestHandler):
        def reply(self, payload, status=200):
            data = json.dumps(payload).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self):
            if self.path == "/api/state":
                try:
                    self.reply(workbench.state())
                except ValueError as e:
                    self.reply({"error": str(e)}, 400)
            elif self.path == "/":
                data = Path(__file__).with_name("alttp_npc_workbench.html").read_bytes()
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)
            else:
                self.reply({"error": "not found"}, 404)

        def do_POST(self):
            # Reject cross-origin requests and stale browser revisions.
            if self.headers.get("Origin") not in (None, f"http://127.0.0.1:{port}"):
                self.reply({"error": "origin mismatch"}, 403)
                return
            try:
                size = int(self.headers.get("Content-Length", 0))
                if not 0 < size <= 4_000_000:
                    raise ValueError("invalid request size")
                body = json.loads(self.rfile.read(size))
                if body["revision"] != workbench.revision:
                    raise ValueError("stale workbench; reload")
                if self.path == "/api/save":
                    edits = body["edits"]
                    if set(edits) - workbench.tiles:
                        raise ValueError("unknown tile identity")
                    for layers in edits.values():
                        validate_layers(layers)
                    atomic_write(output / "edits.json", json.dumps(edits, indent=2) + "\n")
                    workbench.edits = edits
                    workbench.revision += 1
                    self.reply({"revision": workbench.revision})
                elif self.path == "/api/suggest":
                    actor = next(a for a in workbench.manifest["actors"] if a["id"] == body["actor"])
                    if actor.get("graphics_inherited"):
                        raise ValueError("this context inherits graphics slots; verify/reconstruct its entry state before generating defaults")
                    poses = actor["poses"] if body.get("all_poses") else [actor["poses"][body["pose"]]]
                    suggestions, conflicts = {}, set()
                    for pose in poses:
                        for key, proposal in silhouette_defaults(pose).items():
                            if proposal["conflict"] or (key in suggestions and suggestions[key] != proposal["layers"]):
                                conflicts.add(key)
                            suggestions[key] = proposal["layers"]
                    # Heights use a pose-independent local contract; ambiguous
                    # normal estimates remain unauthored until explicitly edited.
                    for key in conflicts:
                        suggestions[key].pop("normal_xyz", None)
                    shared = set(workbench.manifest.get("background_shared_hashes", [])) & suggestions.keys()
                    for key in shared:
                        del suggestions[key]
                    self.reply({"suggestions": suggestions, "normal_conflicts": sorted(conflicts),
                                "background_shared_skipped": sorted(shared)})
                elif self.path == "/api/export":
                    if not profile:
                        raise ValueError("start with --profile to export a runtime profile")
                    text, count = merge_profile(profile.read_text(), workbench.edits)
                    target = output / "npc-profile.toml"
                    candidate = output / "npc-profile-validation.toml"
                    atomic_write(candidate, text)
                    validator = output / "alttp-profile-validate"
                    validation = subprocess.run([str(validator), str(candidate)], capture_output=True, text=True)
                    if validation.returncode:
                        candidate.unlink(missing_ok=True)
                        raise ValueError("runtime profile validation failed: " + validation.stderr)
                    os.replace(candidate, target)
                    self.reply({"path": str(target), "added_layers": count})
                else:
                    self.reply({"error": "not found"}, 404)
            except (ValueError, KeyError, TypeError, IndexError, StopIteration, OSError) as e:
                self.reply({"error": str(e)}, 400)

    # Single-threaded request handling prevents revision/write races.
    from http.server import HTTPServer
    server = HTTPServer(("127.0.0.1", port), Handler)
    url = f"http://127.0.0.1:{port}/"
    print(f"NPC workbench: {url}", flush=True)
    if launch:
        webbrowser.open(url)
    server.serve_forever()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, help="built local room-preview engine with ROM-derived assets")
    parser.add_argument("--atlas", type=Path, default=ROOT / "build/remaster-geometry/alttp-atlas.sqlite")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--generate", action="store_true")
    parser.add_argument("--audit-capture", type=Path, action="append", default=[],
                        help="compare captured OBJ/animation artwork against library (repeatable, read-only)")
    parser.add_argument("--serve", action="store_true")
    parser.add_argument("--open", action="store_true")
    parser.add_argument("--port", type=int, default=8766)
    args = parser.parse_args()
    if args.generate:
        if not args.engine:
            parser.error("--generate requires --engine")
        generate(args.engine.resolve(), args.atlas.resolve(), args.output.resolve())
    if args.serve or args.open:
        serve(args.output.resolve(), args.profile.resolve() if args.profile else None, args.port, args.open)
    elif not args.generate and not args.audit_capture:
        parser.error("choose --generate or --serve/--open")
    if args.audit_capture:
        audit_captures(args.output.resolve(), [p.resolve() for p in args.audit_capture])


if __name__ == "__main__":
    main()
