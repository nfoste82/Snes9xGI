#!/usr/bin/env python3
"""Run a local browser for authoring ALTTP background-tile semantics."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import tempfile
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import threading
import urllib.parse
import webbrowser

from alttp_room_geometry import RoomSnapshot


FORMAT = "alttp-tile-semantics-v1"
ROLES = {"unknown", "floor", "wall", "wall_corner", "stair_top",
         "stair_bottom", "other"}
CARDINAL_NORMALS = {"north", "south", "east", "west"}
NORMALS = CARDINAL_NORMALS | {"north_east", "south_east", "south_west", "north_west"}
CORNER_TYPES = {"inside", "outside"}
MAX_BODY = 64 * 1024
ROOM_INDEX_FORMAT = "alttp-tile-room-occurrences-v2"


class AtlasError(ValueError):
    pass


def validate_annotation(value: object, allow_incomplete: bool = False) -> dict:
    if not isinstance(value, dict):
        raise AtlasError("annotation must be an object")
    allowed = {"role", "normals", "corner_type", "context_dependent", "note"}
    extra = set(value) - allowed
    if extra:
        raise AtlasError("unknown annotation fields: " + ", ".join(sorted(extra)))
    role = value.get("role", "unknown")
    normals = value.get("normals", [])
    corner_type = value.get("corner_type")
    context_dependent = value.get("context_dependent", False)
    note = value.get("note", "")
    if role not in ROLES:
        raise AtlasError(f"unknown role: {role}")
    if (not isinstance(normals, list) or any(item not in NORMALS for item in normals)
            or len(set(normals)) != len(normals)):
        raise AtlasError("normals must be a list of distinct compass directions")
    expected = 1 if role == "wall" else 2 if role == "wall_corner" else 0
    if len(normals) != expected:
        raise AtlasError(f"{role} requires {expected} wall normal(s)")
    if role == "wall_corner":
        if any(normal not in CARDINAL_NORMALS for normal in normals):
            raise AtlasError("wall_corner normals must be cardinal directions")
        if corner_type not in CORNER_TYPES:
            if not allow_incomplete or corner_type not in {None, "unknown"}:
                raise AtlasError("wall_corner requires inside or outside corner_type")
            corner_type = "unknown"
    elif corner_type is not None:
        raise AtlasError("corner_type is only valid for wall_corner")
    if not isinstance(context_dependent, bool):
        raise AtlasError("context_dependent must be boolean")
    if not isinstance(note, str) or len(note) > 4000:
        raise AtlasError("note must be a string no longer than 4000 characters")
    result = {"role": role, "normals": normals,
              "context_dependent": context_dependent, "note": note}
    if role == "wall_corner":
        result["corner_type"] = corner_type
    return result


class AnnotationStore:
    def __init__(self, path: Path, rom_sha256: str):
        self.path = path
        self.rom_sha256 = rom_sha256
        self.lock = threading.Lock()
        self.tiles: dict[str, dict] = {}
        if path.exists():
            value = json.loads(path.read_text())
            if value.get("format") != FORMAT:
                raise AtlasError(f"unsupported annotation format in {path}")
            if value.get("rom_sha256") != rom_sha256:
                raise AtlasError(f"annotation ROM fingerprint differs from atlas: {path}")
            raw_tiles = value.get("tiles")
            if not isinstance(raw_tiles, dict):
                raise AtlasError("annotation file tiles must be an object")
            self.tiles = {key: validate_annotation(item, allow_incomplete=True)
                          for key, item in raw_tiles.items()}

    def document(self) -> dict:
        return {"format": FORMAT, "rom_sha256": self.rom_sha256,
                "tiles": {key: self.tiles[key] for key in sorted(self.tiles)}}

    def save(self, content_id: str, value: object) -> dict:
        return self.save_group([content_id], value)

    def save_group(self, content_ids: list[str], value: object) -> dict:
        annotation = validate_annotation(value)
        with self.lock:
            for content_id in content_ids:
                if (annotation["role"] == "unknown" and not annotation["note"]
                        and not annotation["context_dependent"]):
                    self.tiles.pop(content_id, None)
                else:
                    self.tiles[content_id] = annotation.copy()
            self._write()
        return annotation

    def _write(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.path.with_name(self.path.name + ".tmp")
        temporary.write_text(json.dumps(self.document(), indent=2) + "\n")
        os.replace(temporary, self.path)


class TileCatalog:
    def __init__(self, database: Path):
        self.database = database
        connection = self.connect()
        try:
            info = dict(connection.execute("SELECT key, value FROM run_info"))
            if info.get("atlas_schema_version") != "1":
                raise AtlasError("unsupported or missing atlas schema version")
            self.rom_sha256 = info.get("rom_sha256", "")
            if len(self.rom_sha256) != 64:
                raise AtlasError("atlas has no valid ROM fingerprint")
            slots: dict[tuple[int, int], set[str]] = {}
            conversions: dict[str, set[str]] = {}
            for row in connection.execute(
                    "SELECT pack_index, tile_index, conversion, content_id "
                    "FROM graphics_source WHERE source_kind='background'"):
                slots.setdefault((row["pack_index"], row["tile_index"]), set()).add(
                    row["content_id"])
                conversions.setdefault(row["content_id"], set()).add(row["conversion"])
            parent = {content_id: content_id for content_id in conversions}

            def root(content_id: str) -> str:
                while parent[content_id] != content_id:
                    parent[content_id] = parent[parent[content_id]]
                    content_id = parent[content_id]
                return content_id

            for members in slots.values():
                first = next(iter(members))
                for member in members:
                    parent[root(member)] = root(first)
            groups: dict[str, list[str]] = {}
            for content_id in parent:
                groups.setdefault(root(content_id), []).append(content_id)
            self.aliases = {content_id: sorted(groups[root(content_id)])
                            for content_id in parent}
            self.conversions = conversions
        finally:
            connection.close()


    def connect(self) -> sqlite3.Connection:
        connection = sqlite3.connect(f"file:{self.database.resolve()}?mode=ro", uri=True)
        connection.row_factory = sqlite3.Row
        return connection

    def contains(self, content_id: str) -> bool:
        connection = self.connect()
        try:
            return connection.execute(
                "SELECT 1 FROM decoded_tile d JOIN graphics_source s USING(content_id) "
                "WHERE d.content_id=? AND s.source_kind='background' LIMIT 1",
                (content_id,)).fetchone() is not None
        finally:
            connection.close()

    def semantic_group(self, content_id: str) -> list[str]:
        return self.aliases.get(content_id, [content_id])

    def annotation(self, content_id: str, annotations: dict[str, dict]) -> dict | None:
        direct = annotations.get(content_id)
        if direct is not None:
            return direct
        return next((annotations[alias] for alias in self.semantic_group(content_id)
                     if alias in annotations), None)

    def tiles(self, annotations: dict[str, dict]) -> list[dict]:
        connection = self.connect()
        try:
            rows = connection.execute("""
                WITH bg AS (
                  SELECT content_id, COUNT(*) source_count,
                         COUNT(DISTINCT pack_index) pack_count,
                         GROUP_CONCAT(DISTINCT pack_index) packs,
                         MIN(pack_index) first_pack, MIN(tile_index) first_tile
                  FROM graphics_source WHERE source_kind='background'
                  GROUP BY content_id
                ), families AS (
                  SELECT content_id, COUNT(DISTINCT source_kind) family_count
                  FROM graphics_source GROUP BY content_id
                ), usage AS (
                  SELECT content_id, COUNT(*) placement_count
                  FROM overworld_base_tile_usage WHERE content_id IS NOT NULL
                  GROUP BY content_id
                )
                SELECT d.content_id, d.bit_depth, d.indices, bg.source_count,
                       bg.pack_count, bg.packs, bg.first_pack, bg.first_tile,
                       families.family_count,
                       COALESCE(usage.placement_count, 0) placement_count
                FROM decoded_tile d JOIN bg USING(content_id)
                JOIN families USING(content_id) LEFT JOIN usage USING(content_id)
                ORDER BY bg.first_pack, bg.first_tile, d.content_id
            """).fetchall()
            return [{"content_id": row["content_id"], "bit_depth": row["bit_depth"],
                     "indices": list(row["indices"]),
                     "source_count": row["source_count"],
                     "pack_count": row["pack_count"],
                     "packs": [int(item) for item in row["packs"].split(",")],
                     "first_pack": row["first_pack"],
                     "first_tile": row["first_tile"],
                     "family_count": row["family_count"],
                     "placement_count": row["placement_count"],
                     "conversions": sorted(self.conversions[row["content_id"]]),
                     "aliases": self.semantic_group(row["content_id"]),
                     "low_variant": (self.conversions[row["content_id"]] == {"low"} and
                                     any("high" in self.conversions[alias]
                                         for alias in self.semantic_group(row["content_id"]))),
                     "annotation": self.annotation(row["content_id"], annotations)}
                    for row in rows]
        finally:
            connection.close()

    def detail(self, content_id: str) -> dict | None:
        connection = self.connect()
        try:
            tile = connection.execute(
                "SELECT content_id, bit_depth, indices FROM decoded_tile WHERE content_id=?",
                (content_id,)).fetchone()
            if tile is None:
                return None
            sources = [dict(row) for row in connection.execute(
                "SELECT source_kind, pack_index, tile_index, conversion, snes_address "
                "FROM graphics_source WHERE content_id=? "
                "ORDER BY source_kind, pack_index, conversion, tile_index", (content_id,))]
            if not any(row["source_kind"] == "background" for row in sources):
                return None
            usages = [dict(row) for row in connection.execute(
                "SELECT area_id, quadrant_index, x, y, tile_word, status "
                "FROM overworld_base_tile_usage WHERE content_id=? "
                "ORDER BY area_id, quadrant_index, y, x LIMIT 200", (content_id,))]
            usage_count = connection.execute(
                "SELECT COUNT(*) FROM overworld_base_tile_usage WHERE content_id=?",
                (content_id,)).fetchone()[0]
            alternatives = [dict(row) for row in connection.execute("""
                SELECT DISTINCT sibling.content_id, sibling.conversion,
                       (SELECT COUNT(*) FROM overworld_base_tile_usage usage
                        WHERE usage.content_id=sibling.content_id) overworld_placements
                FROM graphics_source source JOIN graphics_source sibling
                  ON sibling.source_kind='background'
                 AND sibling.pack_index=source.pack_index
                 AND sibling.tile_index=source.tile_index
                 AND sibling.content_id<>source.content_id
                WHERE source.source_kind='background' AND source.content_id=?
                ORDER BY sibling.content_id
            """, (content_id,))]
            return {"content_id": tile["content_id"], "bit_depth": tile["bit_depth"],
                    "indices": list(tile["indices"]), "sources": sources,
                    "usages": usages, "usage_count": usage_count,
                    "usages_truncated": usage_count > len(usages),
                    "alternatives": alternatives}
        finally:
            connection.close()


class RoomOccurrenceIndex:
    def __init__(self, path: Path, rom_sha256: str, renderer: Path | None = None):
        self.path = path
        self.rom_sha256 = rom_sha256
        if not self._valid():
            if renderer is None or not renderer.is_file():
                raise AtlasError(
                    f"room occurrence index is missing; provide --room-renderer to build {path}")
            self._build(renderer.resolve())

    def connect(self) -> sqlite3.Connection:
        connection = sqlite3.connect(f"file:{self.path.resolve()}?mode=ro", uri=True)
        connection.row_factory = sqlite3.Row
        return connection

    def _valid(self) -> bool:
        if not self.path.is_file():
            return False
        try:
            connection = self.connect()
            info = dict(connection.execute("SELECT key, value FROM metadata"))
            count = connection.execute("SELECT COUNT(*) FROM room_status").fetchone()[0]
            connection.close()
            return (info.get("format") == ROOM_INDEX_FORMAT and
                    info.get("rom_sha256") == self.rom_sha256 and count == 320)
        except sqlite3.Error:
            return False

    def _build(self, renderer: Path) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.path.with_name(self.path.name + ".tmp")
        temporary.unlink(missing_ok=True)
        connection = sqlite3.connect(temporary)
        try:
            connection.executescript("""
                PRAGMA journal_mode = OFF;
                PRAGMA synchronous = OFF;
                CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);
                CREATE TABLE occurrence (
                  content_id TEXT NOT NULL, room_id INTEGER NOT NULL,
                  layer INTEGER NOT NULL, x INTEGER NOT NULL, y INTEGER NOT NULL,
                  hflip INTEGER NOT NULL, vflip INTEGER NOT NULL
                );
                CREATE TABLE room_status (
                  room_id INTEGER PRIMARY KEY, status TEXT NOT NULL,
                  detail TEXT NOT NULL
                );
                CREATE TABLE loaded_art (
                  content_id TEXT NOT NULL, room_id INTEGER NOT NULL,
                  PRIMARY KEY (content_id, room_id)
                );
            """)
            connection.executemany("INSERT INTO metadata VALUES (?, ?)",
                                   [("format", ROOM_INDEX_FORMAT),
                                    ("rom_sha256", self.rom_sha256)])
            with tempfile.TemporaryDirectory() as directory:
                raw = Path(directory) / "room.raw"
                for room in range(320):
                    result = subprocess.run(
                        [str(renderer), "--linked", hex(room), str(raw),
                         "-1", "0", "0", "0"], cwd=renderer.parent,
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                    if result.returncode:
                        connection.execute("INSERT INTO room_status VALUES (?, ?, ?)",
                                           (room, "unsupported", result.stderr.strip()[-1000:]))
                        continue
                    snapshot = RoomSnapshot.read(raw)
                    connection.executemany(
                        "INSERT INTO occurrence VALUES (?, ?, ?, ?, ?, ?, ?)",
                        ((cell.tile_hash, room, cell.layer, cell.x, cell.y,
                          int(cell.hflip), int(cell.vflip))
                         for layer in snapshot.cells for cell in layer))
                    # Both dungeon BGs use the same 1024-tile 4bpp character
                    # region. Keep loaded-but-unreferenced art distinct from
                    # actual tilemap placements; event code may reveal it later.
                    connection.executemany(
                        "INSERT OR IGNORE INTO loaded_art VALUES (?, ?)",
                        ((snapshot._tile_hash(address), room)
                         for address in range(snapshot.layers[0][1],
                                              snapshot.layers[0][1] + 0x4000, 16)))
                    connection.execute("INSERT INTO room_status VALUES (?, ?, ?)",
                                       (room, "indexed", result.stdout.strip()))
                    for suffix in ("", ".prov", ".attr", ".state.json"):
                        Path(str(raw) + suffix).unlink(missing_ok=True)
                    if (room + 1) % 20 == 0:
                        print(f"Indexed {room + 1}/320 dungeon rooms", flush=True)
            connection.executescript("""
                CREATE INDEX occurrence_content_room ON occurrence(content_id, room_id);
                CREATE INDEX occurrence_room ON occurrence(room_id);
                CREATE INDEX loaded_art_content ON loaded_art(content_id);
            """)
            connection.commit()
        except Exception:
            connection.close()
            temporary.unlink(missing_ok=True)
            raise
        else:
            connection.close()
            os.replace(temporary, self.path)

    def rooms(self, content_id: str) -> list[dict]:
        connection = self.connect()
        try:
            return [dict(row) for row in connection.execute("""
                SELECT room_id, COUNT(*) placement_count,
                       SUM(layer=0) bg1_count, SUM(layer=1) bg2_count
                FROM occurrence WHERE content_id=? GROUP BY room_id
                ORDER BY room_id=27, placement_count DESC, room_id
            """, (content_id,))]
        finally:
            connection.close()

    def loaded_rooms(self, content_id: str) -> list[int]:
        connection = self.connect()
        try:
            return [row[0] for row in connection.execute(
                "SELECT room_id FROM loaded_art WHERE content_id=? ORDER BY room_id=27, room_id",
                (content_id,))]
        finally:
            connection.close()

    def cells(self, content_id: str, room_id: int) -> list[dict]:
        connection = self.connect()
        try:
            return [dict(row) for row in connection.execute(
                "SELECT layer, x, y, hflip, vflip FROM occurrence "
                "WHERE content_id=? AND room_id=? ORDER BY layer, y, x",
                (content_id, room_id))]
        finally:
            connection.close()


PAGE = r'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ALTTP Tile Semantics</title><style>
:root{color-scheme:dark;--ink:#e8e4da;--muted:#a9a397;--line:#49483f;--panel:#242620;--paper:#171914;--accent:#d6b86c;--blue:#84b9c4;--bad:#d68778}*{box-sizing:border-box}body{margin:0;height:100vh;overflow:hidden;background:var(--paper);color:var(--ink);font:14px/1.35 ui-monospace,SFMono-Regular,Menlo,monospace}button,input,select,textarea{font:inherit;color:inherit}header{height:76px;padding:16px 22px;border-bottom:1px solid var(--line);background:#1d201a;display:flex;align-items:center;justify-content:space-between;gap:20px}h1{font:700 24px/1.1 Georgia,serif;letter-spacing:.03em;margin:0}.sub{color:var(--muted);margin-top:5px}.app{height:calc(100vh - 76px);display:grid;grid-template-columns:250px minmax(350px,1fr) 430px}.filters,.gridPane,.detail{overflow:auto}.filters{padding:18px;border-right:1px solid var(--line);background:#1d201a}.gridPane{padding:16px}.detail{padding:20px;border-left:1px solid var(--line);background:#20221d}label.cap{display:block;color:var(--muted);font-weight:bold;text-transform:uppercase;letter-spacing:.08em;font-size:11px;margin:0 0 6px}.field{margin-bottom:17px}input[type=search],select,textarea{width:100%;background:#11130f;border:1px solid var(--line);border-radius:4px;padding:9px}textarea{resize:vertical;min-height:80px}.checks{display:grid;gap:8px}.checks label{display:flex;align-items:center;gap:8px}.checks input{accent-color:var(--accent)}.stats{padding:10px 0;color:var(--accent)}.hint,.status{color:var(--muted);font-size:12px}.tileGrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(92px,1fr));gap:9px}.tile{background:var(--panel);border:1px solid #383b32;border-radius:5px;padding:8px;min-width:0;text-align:left;cursor:pointer}.tile:hover,.tile.selected{border-color:var(--accent)}.tile canvas{display:block;width:64px;height:64px;margin:auto;image-rendering:pixelated;background:#0d0e0c}.tile .role{height:18px;margin-top:6px;overflow:hidden;color:var(--blue);font-size:11px;text-align:center}.tile .id{overflow:hidden;text-overflow:ellipsis;color:var(--muted);font-size:9px;white-space:nowrap}.badges{height:16px;text-align:center;white-space:nowrap;overflow:hidden}.badge{display:inline-block;color:#171914;background:var(--accent);padding:1px 4px;border-radius:2px;font-size:9px;margin-right:3px}.badge.warn{background:var(--bad)}.big{width:96px;height:96px;image-rendering:pixelated;background:#0d0e0c;border:1px solid var(--line);display:block;margin:0 0 12px}.roomContext{margin-bottom:20px;padding-bottom:18px;border-bottom:1px solid var(--line)}.roomContext canvas{display:block;width:100%;height:auto;aspect-ratio:1;image-rendering:pixelated;background:#0d0e0c;border:1px solid var(--line);margin-top:9px}.roomLegend{display:flex;gap:15px;margin:7px 0;color:var(--muted);font-size:11px}.roomLegend i{display:inline-block;width:10px;height:10px;margin-right:5px}.roleButtons{display:grid;grid-template-columns:1fr 1fr;gap:6px}.roleButtons button,.normalButtons button,.cornerButtons button,.action{background:#30332b;border:1px solid var(--line);border-radius:4px;padding:8px;cursor:pointer}.roleButtons button.on,.normalButtons button.on,.cornerButtons button.on{background:#685c38;border-color:var(--accent)}.normalButtons{display:grid;grid-template-columns:repeat(4,1fr);gap:6px}.cornerButtons{display:grid;grid-template-columns:1fr 1fr;gap:6px}.normalBox{margin-top:16px}.meta{display:grid;grid-template-columns:auto 1fr;gap:5px 12px;margin:16px 0}.meta span{color:var(--muted)}.sources{font-size:11px;border-top:1px solid var(--line);margin-top:15px;padding-top:12px}.source{margin:5px 0;color:var(--muted)}.saveState{min-height:18px;color:var(--blue);margin-top:8px}.empty{color:var(--muted);padding:30px}.context{display:flex;gap:8px;align-items:flex-start;margin-top:14px}.export{background:#30332b;border:1px solid var(--line);border-radius:4px;padding:8px 12px;cursor:pointer}@media(max-width:900px){body{height:auto;overflow:auto}.app{height:auto;display:block}.filters,.gridPane,.detail{overflow:visible}.detail{border-left:0;border-top:1px solid var(--line)}}
</style></head><body><header><div><h1>ALTTP Tile Semantics</h1><div class="sub" id="subtitle">Loading canonical background artwork...</div></div><button class="export" id="export">Export JSON</button></header><div class="app">
<aside class="filters"><div class="field"><label class="cap" for="search">Find tile</label><input id="search" type="search" placeholder="hash, role, note"></div><div class="field"><label class="cap" for="roleFilter">Role</label><select id="roleFilter"><option value="">All roles</option><option>unknown</option><option>floor</option><option>wall</option><option>wall_corner</option><option>stair_top</option><option>stair_bottom</option><option>other</option></select></div><div class="field"><label class="cap" for="packFilter">Graphics pack</label><select id="packFilter"><option value="">All packs</option></select></div><div class="checks"><label><input id="reviewed" type="checkbox"> Reviewed only</label><label><input id="reused" type="checkbox"> Reused artwork</label><label><input id="contextOnly" type="checkbox"> Context-dependent</label><label><input id="showLow" type="checkbox"> Show low/grayscale variants</label></div><div class="stats" id="stats"></div><p class="hint">Low and high variants are exact-source aliases created by the game's two 3bpp conversion paths. They share semantics; low variants are hidden by default. Artwork uses a diagnostic palette, not in-game CGRAM colors.</p></aside>
<main class="gridPane"><div class="tileGrid" id="grid"></div></main><section class="detail" id="detail"><div class="empty">Select a tile to annotate it.</div></section></div>
<script>
const roles=['unknown','floor','wall','wall_corner','stair_top','stair_bottom','other'];const labels={unknown:'Unknown',floor:'Floor',wall:'Wall',wall_corner:'Wall corner',stair_top:'Stair top (high)',stair_bottom:'Stair bottom (low)',other:'Other / non-geometry'};const palette=['#10110f','#36382f','#555747','#74765f','#939579','#b2b894','#d2d7ad','#f0ebcb','#3e5355','#55777a','#6f9da0','#92c5c6','#5d4936','#85633e','#b68b4f','#e1bd70'];let meta,tiles=[],selected=null,detailData=null,saveTimer=null,pendingId=null,pageSize=1000;const roomChoice=new Map(),byId=new Map();const grid=document.getElementById('grid'),detail=document.getElementById('detail');
function draw(canvas,indices){canvas.width=8;canvas.height=8;let c=canvas.getContext('2d'),im=c.createImageData(8,8);for(let i=0;i<64;i++){let color=palette[indices[i]%palette.length],n=parseInt(color.slice(1),16);im.data[i*4]=n>>16;im.data[i*4+1]=(n>>8)&255;im.data[i*4+2]=n&255;im.data[i*4+3]=255}c.putImageData(im,0,0)}
function roleOf(t){return t.annotation?.role||'unknown'}function filtered(){let q=document.getElementById('search').value.trim().toLowerCase(),r=document.getElementById('roleFilter').value,p=document.getElementById('packFilter').value;return tiles.filter(t=>{let a=t.annotation||{};if(t.low_variant&&!document.getElementById('showLow').checked)return false;if(r&&roleOf(t)!==r)return false;if(p&&!t.packs.includes(Number(p)))return false;if(document.getElementById('reviewed').checked&&!t.annotation)return false;if(document.getElementById('reused').checked&&t.source_count<2&&t.family_count<2)return false;if(document.getElementById('contextOnly').checked&&!a.context_dependent)return false;return !q||[t.content_id,...t.aliases,a.role,a.corner_type,a.note,(a.normals||[]).join(' ')].join(' ').toLowerCase().includes(q)})}
function renderGrid(){let list=filtered(),shown=list.slice(0,pageSize);document.getElementById('stats').textContent=`${shown.length} shown · ${list.length} match · ${tiles.length} runtime identities`;grid.replaceChildren();let f=document.createDocumentFragment();for(let t of shown){let b=document.createElement('button');b.className='tile'+(selected===t.content_id?' selected':'');b.onclick=()=>select(t.content_id);let c=document.createElement('canvas');draw(c,t.indices);b.append(c);let badges=document.createElement('div');badges.className='badges';let pack=document.createElement('span');pack.className='badge';pack.textContent='P'+t.first_pack+':'+t.first_tile;badges.append(pack);if(t.low_variant){let x=document.createElement('span');x.className='badge';x.textContent='low alias';badges.append(x)}else if(t.aliases.length===1&&t.source_count>1){let x=document.createElement('span');x.className='badge';x.textContent=t.source_count+' src';badges.append(x)}if(t.family_count>1){let x=document.createElement('span');x.className='badge warn';x.textContent='shared';badges.append(x)}b.append(badges);let role=document.createElement('div');role.className='role';role.textContent=labels[roleOf(t)];b.append(role);let id=document.createElement('div');id.className='id';id.textContent=t.content_id;b.append(id);f.append(b)}grid.append(f);if(shown.length<list.length){let more=document.createElement('button');more.className='action';more.textContent=`Show next ${Math.min(1000,list.length-shown.length)} tiles`;more.onclick=()=>{pageSize+=1000;renderGrid()};grid.append(more)}}
async function select(id){if(pendingId){clearTimeout(saveTimer);await save(pendingId)}selected=id;history.replaceState(null,'','?tile='+encodeURIComponent(id));renderGrid();detail.innerHTML='<div class="empty">Loading...</div>';detailData=await fetch('/api/tile/'+encodeURIComponent(id)).then(r=>r.json());renderDetail()}
function annotation(){let t=byId.get(selected);return t.annotation||{role:'unknown',normals:[],context_dependent:false,note:''}}
function renderDetail(){let t=byId.get(selected),a=annotation();detail.replaceChildren();renderRoomContext();let canvas=document.createElement('canvas');canvas.className='big';draw(canvas,t.indices);detail.append(canvas);let title=document.createElement('div');title.textContent=selected;title.style.overflowWrap='anywhere';detail.append(title);let metaBox=document.createElement('div');metaBox.className='meta';for(let [k,v] of [['Bit depth',t.bit_depth],['ROM sources',t.source_count],['Background packs',t.pack_count],['Source families',t.family_count],['Overworld placements',t.placement_count]]){let s=document.createElement('span');s.textContent=k;let b=document.createElement('b');b.textContent=v;metaBox.append(s,b)}detail.append(metaBox);let cap=document.createElement('label');cap.className='cap';cap.textContent='Tile role';detail.append(cap);let rb=document.createElement('div');rb.className='roleButtons';for(let role of roles){let b=document.createElement('button');b.textContent=labels[role];b.className=a.role===role?'on':'';b.onclick=()=>changeRole(role);rb.append(b)}detail.append(rb);if(a.role==='wall'||a.role==='wall_corner')renderNormals(a);if(a.role==='wall_corner')renderCornerType(a);let context=document.createElement('label');context.className='context';context.innerHTML=`<input type="checkbox" ${a.context_dependent?'checked':''}><span>Context-dependent use: this artwork has incompatible meanings in different placements.</span>`;context.querySelector('input').onchange=e=>update({...a,context_dependent:e.target.checked});detail.append(context);let noteCap=document.createElement('label');noteCap.className='cap';noteCap.style.marginTop='16px';noteCap.textContent='Notes';detail.append(noteCap);let note=document.createElement('textarea');note.value=a.note;note.placeholder='Optional evidence or usage caveat';note.oninput=()=>{let current=annotation();byId.get(selected).annotation={...current,note:note.value};scheduleSave()};detail.append(note);let state=document.createElement('div');state.className='saveState';state.id='saveState';detail.append(state);showCompletionState(a);let src=document.createElement('div');src.className='sources';src.innerHTML='<label class="cap">ROM provenance</label>';for(let s of detailData.sources){let d=document.createElement('div');d.className='source';d.textContent=`${s.source_kind} pack ${s.pack_index??'-'} tile ${s.tile_index} ${s.conversion} @ $${s.snes_address.toString(16)}`;src.append(d)}if(detailData.usages_truncated){let d=document.createElement('div');d.className='source';d.textContent=`Showing 200 of ${detailData.usage_count} overworld placements.`;src.append(d)}detail.append(src)}
function renderNormals(a){let corner=a.role==='wall_corner',box=document.createElement('div');box.className='normalBox';box.innerHTML='<label class="cap">Wall normal(s), default orientation</label>';let nb=document.createElement('div');nb.className='normalButtons';let directions=corner?[['north','N'],['east','E'],['south','S'],['west','W']]:[['north','N'],['north_east','NE'],['east','E'],['south_east','SE'],['south','S'],['south_west','SW'],['west','W'],['north_west','NW']];for(let [n,label] of directions){let b=document.createElement('button');b.textContent=label;b.title=n.replace('_',' ');b.className=a.normals.includes(n)?'on':'';b.onclick=()=>toggleNormal(n);nb.append(b)}box.append(nb);detail.append(box)}
function renderCornerType(a){let box=document.createElement('div');box.className='normalBox';box.innerHTML='<label class="cap">Corner type</label>';let buttons=document.createElement('div');buttons.className='cornerButtons';for(let type of ['inside','outside']){let b=document.createElement('button');b.textContent=type[0].toUpperCase()+type.slice(1);b.className=a.corner_type===type?'on':'';b.onclick=()=>update({...a,corner_type:type});buttons.append(b)}box.append(buttons);detail.append(box)}
function renderRoomContext(){let box=document.createElement('div');box.className='roomContext';let cap=document.createElement('label');cap.className='cap';cap.textContent='Dungeon room context';box.append(cap);if(!detailData.rooms.length){let p=document.createElement('div');p.className='hint';let activeAlt=detailData.alternatives.find(a=>a.dungeon_rooms||a.loaded_rooms||a.overworld_placements);if(detailData.loaded_rooms.length)p.textContent=`Loaded in ${detailData.loaded_rooms.length} indexed dungeon graphics context${detailData.loaded_rooms.length===1?'':'s'}, but no default-state BG1/BG2 tilemap references it. It may be event or replacement art revealed by another room state.`;else if(detailData.usage_count)p.textContent=`No indexed dungeon preview. This tile is used ${detailData.usage_count} times in the overworld base tilemap, so it is not unused art.`;else if(activeAlt)p.textContent=`No placement for this palette-half conversion. The same ROM pack/tile slot has an active ${activeAlt.conversion} conversion (${activeAlt.dungeon_rooms} dungeon rooms, ${activeAlt.loaded_rooms} loaded contexts, ${activeAlt.overworld_placements} overworld placements).`;else p.textContent='No indexed dungeon or overworld-base placement was found. It may be unused decoded art, used only in an unsampled state, or referenced by another map system.';box.append(p);detail.append(box);return}let chosen=roomChoice.get(selected);if(!detailData.rooms.some(r=>r.room_id===chosen))chosen=detailData.rooms[0].room_id;roomChoice.set(selected,chosen);let select=document.createElement('select');for(let r of detailData.rooms){let o=document.createElement('option');o.value=r.room_id;o.selected=r.room_id===chosen;o.textContent=`Room 0x${r.room_id.toString(16).padStart(3,'0')} · ${r.placement_count} placements · BG1 ${r.bg1_count} / BG2 ${r.bg2_count}`;select.append(o)}select.onchange=()=>{roomChoice.set(selected,Number(select.value));renderDetail()};box.append(select);let canvas=document.createElement('canvas');canvas.width=512;canvas.height=512;box.append(canvas);let legend=document.createElement('div');legend.className='roomLegend';legend.innerHTML='<span><i style="background:#35e6ff"></i>BG1</span><span><i style="background:#ffd23f"></i>BG2</span>';box.append(legend);let note=document.createElement('div');note.className='hint';note.textContent='Exact 8x8 tilemap placements in the renderer’s default room state. Room 0x01b is placed last because its stairs are initially obscured.';box.append(note);detail.append(box);loadRoom(canvas,chosen)}
async function loadRoom(canvas,room){let id=selected,response=await fetch(`/api/room/${room}/${encodeURIComponent(id)}`),data=await response.json();if(id!==selected)return;let image=new Image();image.onload=()=>{let c=canvas.getContext('2d');c.drawImage(image,0,0,512,512);c.lineWidth=2;for(let cell of data.cells){c.fillStyle=cell.layer?'rgba(255,210,63,.36)':'rgba(53,230,255,.36)';c.strokeStyle=cell.layer?'#ffd23f':'#35e6ff';c.fillRect(cell.x*8,cell.y*8,8,8);c.strokeRect(cell.x*8+1,cell.y*8+1,6,6)}};image.src=`/room-image/${room.toString(16).padStart(3,'0')}.png`}
function complete(a){let count=a.role==='wall'?1:a.role==='wall_corner'?2:0;return a.normals.length===count&&(a.role!=='wall_corner'||['inside','outside'].includes(a.corner_type))}function showCompletionState(a){let state=document.getElementById('saveState');if(!state||complete(a))return;if(a.normals.length<(a.role==='wall_corner'?2:1))state.textContent='Choose the required wall normal'+(a.role==='wall_corner'?'s':'')+' to save';else if(a.role==='wall_corner')state.textContent='Choose inside or outside to save'}
function changeRole(role){let a={...annotation(),role,normals:[]};delete a.corner_type;if(role==='wall_corner')a.corner_type='unknown';byId.get(selected).annotation=a;renderDetail();if(complete(a))scheduleSave()}function toggleNormal(n){let a=annotation(),count=a.role==='wall'?1:2,normals=a.normals.includes(n)?a.normals.filter(x=>x!==n):[...a.normals,n].slice(-count);byId.get(selected).annotation={...a,normals};renderDetail();if(complete({...a,normals}))scheduleSave()}
function update(a){byId.get(selected).annotation=a;renderDetail();if(complete(a))scheduleSave()}function scheduleSave(){clearTimeout(saveTimer);pendingId=selected;let state=document.getElementById('saveState');if(state)state.textContent='Unsaved...';saveTimer=setTimeout(()=>save(pendingId),350)}async function save(id){if(!id)return;let t=byId.get(id),snapshot=t.annotation;if(!complete(snapshot)){pendingId=null;return}let response=await fetch('/api/annotation/'+encodeURIComponent(id),{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify(snapshot)}),value=await response.json();if(pendingId===id)pendingId=null;let state=id===selected?document.getElementById('saveState'):null;if(!response.ok){if(state)state.textContent='Not saved: '+value.error;return}let stored=(value.annotation.role==='unknown'&&!value.annotation.note&&!value.annotation.context_dependent)?null:value.annotation;for(let alias of value.content_ids)if(byId.has(alias))byId.get(alias).annotation=stored;if(state)state.textContent=`Saved to ${value.content_ids.length} runtime identit${value.content_ids.length===1?'y':'ies'}`;renderGrid()}
for(let id of ['search','roleFilter','packFilter','reviewed','reused','contextOnly','showLow'])document.getElementById(id).addEventListener(id==='search'?'input':'change',()=>{pageSize=1000;renderGrid()});document.getElementById('export').onclick=()=>location.href='/api/export';
fetch('/api/catalog').then(r=>r.json()).then(data=>{meta=data;tiles=data.tiles;for(let t of tiles)byId.set(t.content_id,t);let packs=[...new Set(tiles.flatMap(t=>t.packs))].sort((a,b)=>a-b),select=document.getElementById('packFilter');for(let pack of packs){let option=document.createElement('option');option.value=pack;option.textContent='Pack '+pack;select.append(option)}document.getElementById('subtitle').textContent=`${tiles.length} unique background tiles · ROM ${data.rom_sha256.slice(0,12)}… · autosaves to ${data.annotation_path}`;renderGrid();let wanted=new URLSearchParams(location.search).get('tile');if(wanted&&byId.has(wanted))select(wanted)}).catch(e=>{document.getElementById('subtitle').textContent='Could not load atlas: '+e});
</script></body></html>'''


def make_handler(catalog: TileCatalog, store: AnnotationStore,
                 room_index: RoomOccurrenceIndex, room_images: Path):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, pattern: str, *args) -> None:
            print(f"{self.address_string()} - {pattern % args}")

        def json_response(self, value: object, status: HTTPStatus = HTTPStatus.OK,
                          attachment: bool = False) -> None:
            body = json.dumps(value, separators=(",", ":")).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            if attachment:
                self.send_header("Content-Disposition",
                                 'attachment; filename="alttp-tile-semantics-v1.json"')
            self.end_headers()
            self.wfile.write(body)

        def error(self, status: HTTPStatus, message: str) -> None:
            self.json_response({"error": message}, status)

        def do_GET(self) -> None:
            path = urllib.parse.urlsplit(self.path).path
            if path == "/":
                body = PAGE.encode()
                self.send_response(HTTPStatus.OK)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            elif path == "/api/catalog":
                self.json_response({"format": FORMAT, "rom_sha256": catalog.rom_sha256,
                                    "annotation_path": str(store.path),
                                    "tiles": catalog.tiles(store.tiles)})
            elif path == "/api/export":
                self.json_response(store.document(), attachment=True)
            elif path.startswith("/api/tile/"):
                content_id = urllib.parse.unquote(path[len("/api/tile/"):])
                detail = catalog.detail(content_id)
                if detail is None:
                    self.error(HTTPStatus.NOT_FOUND, "unknown background tile")
                else:
                    aliases = catalog.semantic_group(content_id)
                    rooms: dict[int, dict] = {}
                    loaded_rooms: set[int] = set()
                    for alias in aliases:
                        for room in room_index.rooms(alias):
                            current = rooms.setdefault(room["room_id"], {
                                "room_id": room["room_id"], "placement_count": 0,
                                "bg1_count": 0, "bg2_count": 0, "content_ids": []})
                            current["placement_count"] += room["placement_count"]
                            current["bg1_count"] += room["bg1_count"]
                            current["bg2_count"] += room["bg2_count"]
                            current["content_ids"].append(alias)
                        loaded_rooms.update(room_index.loaded_rooms(alias))
                    detail["rooms"] = sorted(rooms.values(), key=lambda room: (
                        room["room_id"] == 27, -room["placement_count"], room["room_id"]))
                    detail["loaded_rooms"] = sorted(loaded_rooms)
                    detail["semantic_aliases"] = aliases
                    for alternative in detail["alternatives"]:
                        alternative["dungeon_rooms"] = len(
                            room_index.rooms(alternative["content_id"]))
                        alternative["loaded_rooms"] = len(
                            room_index.loaded_rooms(alternative["content_id"]))
                    self.json_response(detail)
            elif path.startswith("/api/room/"):
                parts = path.split("/", 4)
                try:
                    room_id = int(parts[3])
                    content_id = urllib.parse.unquote(parts[4])
                except (ValueError, IndexError):
                    self.error(HTTPStatus.BAD_REQUEST, "invalid room occurrence request")
                    return
                if not 0 <= room_id < 320 or not catalog.contains(content_id):
                    self.error(HTTPStatus.NOT_FOUND, "unknown room or background tile")
                else:
                    cells = []
                    for alias in catalog.semantic_group(content_id):
                        cells.extend(room_index.cells(alias, room_id))
                    self.json_response({"room_id": room_id, "content_id": content_id,
                                        "cells": cells})
            elif path.startswith("/room-image/") and path.endswith(".png"):
                name = path[len("/room-image/"):]
                if (len(name) != 7 or not name[:3].isalnum() or name[3:] != ".png"):
                    self.error(HTTPStatus.NOT_FOUND, "unknown room image")
                    return
                image = room_images / f"room-{name}"
                if not image.is_file():
                    self.error(HTTPStatus.NOT_FOUND, "room image is unavailable")
                    return
                body = image.read_bytes()
                self.send_response(HTTPStatus.OK)
                self.send_header("Content-Type", "image/png")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            else:
                self.error(HTTPStatus.NOT_FOUND, "not found")

        def do_PUT(self) -> None:
            path = urllib.parse.urlsplit(self.path).path
            if not path.startswith("/api/annotation/"):
                self.error(HTTPStatus.NOT_FOUND, "not found")
                return
            content_id = urllib.parse.unquote(path[len("/api/annotation/"):])
            if not catalog.contains(content_id):
                self.error(HTTPStatus.NOT_FOUND, "unknown background tile")
                return
            try:
                length = int(self.headers.get("Content-Length", "0"))
                if length <= 0 or length > MAX_BODY:
                    raise AtlasError("invalid request body size")
                value = json.loads(self.rfile.read(length))
                aliases = catalog.semantic_group(content_id)
                annotation = store.save_group(aliases, value)
                self.json_response({"annotation": annotation, "content_ids": aliases})
            except (AtlasError, json.JSONDecodeError, UnicodeDecodeError) as exc:
                self.error(HTTPStatus.BAD_REQUEST, str(exc))

    return Handler


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--atlas", type=Path, required=True)
    parser.add_argument("--annotations", type=Path,
                        default=Path("tools/alttp-tile-semantics-v1.json"))
    parser.add_argument("--room-index", type=Path,
                        default=Path("build/remaster-geometry/tile-room-occurrences.sqlite"))
    parser.add_argument("--room-renderer", type=Path,
                        default=Path("build/remaster-geometry/room-preview-engine-v21/alttp-room-preview"),
                        help="linked reference renderer, used only when building the room index")
    parser.add_argument("--room-images", type=Path,
                        default=Path("build/remaster-geometry/room-gallery-room-local/images"))
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--open", action="store_true", help="open the browser")
    args = parser.parse_args()
    catalog = TileCatalog(args.atlas)
    store = AnnotationStore(args.annotations, catalog.rom_sha256)
    room_images = args.room_images.resolve()
    if not room_images.is_dir():
        parser.error(f"room image directory does not exist: {room_images}")
    room_index = RoomOccurrenceIndex(args.room_index, catalog.rom_sha256,
                                     args.room_renderer)
    server = ThreadingHTTPServer(("127.0.0.1", args.port),
                                 make_handler(catalog, store, room_index, room_images))
    url = f"http://127.0.0.1:{server.server_port}/"
    print(f"ALTTP tile semantic atlas: {url}")
    print(f"Annotations autosave to: {store.path.resolve()}")
    if args.open:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
