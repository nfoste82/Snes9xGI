"""ROM-fingerprinted, offline artwork and placement cache for the semantic atlas."""
from __future__ import annotations

from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import struct
import tempfile


WORLD_FORMAT = "alttp-tile-semantic-world-v2"
SIDE = 512
# Little endian: content table index (0 = unresolved), Map16, area, flip bits.
CELL = struct.Struct("<IHBB")


class WorldCache:
    def __init__(self, path, catalog, rom_path=None):
        self.path = Path(path)
        self.catalog = catalog
        if not self.valid():
            if rom_path is None:
                raise ValueError("world cache missing or invalid; provide --rom to rebuild it")
            self.build(Path(rom_path).expanduser())
        self.manifest = json.loads((self.path / "manifest.json").read_text())
        self.grids = {world: (self.path / f"{world}.bin").read_bytes()
                      for world in ("light", "dark")}

    def valid(self):
        try:
            info = json.loads((self.path / "manifest.json").read_text())
            if (info["format"] != WORLD_FORMAT or
                    info["rom_sha256"] != self.catalog.rom_sha256 or
                    info["atlas_sha256"] != hashlib.sha256(self.catalog.database.read_bytes()).hexdigest()):
                return False
            for name, digest in info["files"].items():
                if name not in {"light.png", "dark.png", "light.bin", "dark.bin"}:
                    return False
                if hashlib.sha256((self.path / name).read_bytes()).hexdigest() != digest:
                    return False
            return set(info["files"]) == {"light.png", "dark.png", "light.bin", "dark.bin"}
        except (OSError, ValueError, KeyError, TypeError):
            return False

    def build(self, rom_path):
        try:
            from PIL import Image
            from alttp_visual_context import rom_palette, tile_stamp
        except ImportError as error:
            raise ValueError("Building the world map requires Pillow: install with python3 -m pip install Pillow, or use a Python environment with Pillow") from error
        from alttp_atlas import Rom
        from alttp_atlas_graphics import extract_graphics
        from alttp_atlas_overworld import extract_overworld
        from alttp_atlas_overworld_gfx import BaseOverworldGraphics
        from alttp_atlas_world import catalog_world

        print("Building Light/Dark World semantic map cache (base ROM, animation phase 0)…", flush=True)
        rom = Rom(rom_path)
        if rom.sha256 != self.catalog.rom_sha256:
            raise ValueError("world ROM fingerprint differs from atlas")
        quadrants = extract_overworld(rom)
        graphics = extract_graphics(rom)
        resolver = BaseOverworldGraphics(graphics, catalog_world(rom)["overworld_areas"], quadrants)
        by_id = {g.content_id: g.indices for g in graphics}
        connection = self.catalog.connect()
        try:
            rows = [dict(row) for row in connection.execute("SELECT * FROM overworld_base_tile_usage ORDER BY quadrant_index, y, x")]
        finally:
            connection.close()
        ids = [None] + sorted({r["content_id"] for r in rows if r["content_id"]})
        id_index = {key: i for i, key in enumerate(ids)}
        images = {w: Image.new("RGB", (4096, 4096), "magenta") for w in ("light", "dark")}
        grids = {w: bytearray(CELL.pack(0, 0, 255, 0) * SIDE * SIDE) for w in images}
        tile_maps = {qi: {(t.x, t.y): t for t in quadrants[qi].iter_tiles()} for qi in range(128)}
        palettes, stamps = {}, {}
        counts = {w: Counter() for w in images}
        for n, row in enumerate(rows):
            qi, x, y, area = (row[k] for k in ("quadrant_index", "x", "y", "area_id"))
            if not 0 <= qi < 128:
                continue
            world = "dark" if qi >= 64 else "light"
            wx, wy = (qi & 7) * 64 + x, ((qi & 63) >> 3) * 64 + y
            tile = tile_maps[qi][x, y]
            resolved = resolver.resolve_tile(area, tile)
            if resolved.status == "animated_vram_slot":
                resolved = resolver.resolve_animated_phase(resolved, 0)
            # Selection, counts and rendering must describe the same artwork.
            # Base usage deliberately has no identity for animated VRAM slots.
            key = resolved.content_id
            if key not in id_index:
                id_index[key] = len(ids)
                ids.append(key)
            CELL.pack_into(grids[world], (wy * SIDE + wx) * CELL.size,
                           id_index[key], tile.map16_id, area,
                           int(tile.flip_x) | (int(tile.flip_y) << 1))
            counts[world][key] += 1
            if area not in palettes:
                palettes[area] = rom_palette(rom, area, rom.get_byte(0x80FD1C + area))[0]
            if resolved.content_id is not None:
                stamp_key = (area, resolved.content_id, tile.palette, tile.flip_x, tile.flip_y)
                if stamp_key not in stamps:
                    stamps[stamp_key] = tile_stamp(by_id[resolved.content_id], tile, palettes[area])
                images[world].paste(stamps[stamp_key], (wx * 8, wy * 8))
            if (n + 1) % 65536 == 0:
                print(f"  rendered {n + 1:,}/{len(rows):,} world cells", flush=True)
        self.path.mkdir(parents=True, exist_ok=True)
        # Publish the manifest last; partial/interrupted output fails validation.
        with tempfile.TemporaryDirectory(dir=self.path) as temporary:
            stage = Path(temporary)
            for world in images:
                images[world].save(stage / f"{world}.png")
                (stage / f"{world}.bin").write_bytes(grids[world])
            files = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in stage.iterdir()}
            manifest = dict(format=WORLD_FORMAT, rom_sha256=rom.sha256,
                            atlas_sha256=hashlib.sha256(self.catalog.database.read_bytes()).hexdigest(),
                            files=files, size=SIDE, cell_size=8, record_bytes=CELL.size,
                            content_ids=ids, counts={w: dict(c) for w, c in counts.items()},
                            limitations="Base ROM only, no events/actors; artwork animation phase 0. Magenta = unresolved artwork; unknown palette slots use grayscale. Unresolved placement identities remain unselectable.")
            (stage / "manifest.json").write_text(json.dumps(manifest, separators=(",", ":")))
            for name in [*files, "manifest.json"]:
                os.replace(stage / name, self.path / name)
        print(f"World cache ready: {self.path}", flush=True)

    def cell(self, world, x, y):
        if world not in self.grids or not 0 <= x < SIDE or not 0 <= y < SIDE:
            raise ValueError("unknown world or cell outside 0..511")
        index, map16, area, flips = CELL.unpack_from(self.grids[world], (y * SIDE + x) * CELL.size)
        key = self.manifest["content_ids"][index]
        aliases = self.catalog.semantic_group(key) if key else []
        # Prefer the visible high-half identity, matching the tile grid's default.
        target = next((a for a in aliases if "high" in self.catalog.conversions.get(a, set())),
                      next((a for a in aliases if a in self.catalog.aliases), None))
        return dict(world=world, x=x, y=y, pixel_x=x * 8, pixel_y=y * 8,
                    area_id=area, map16_id=map16, flip_x=bool(flips & 1), flip_y=bool(flips & 2),
                    content_id=key, catalog_content_id=target,
                    placement_count=sum(self.manifest["counts"][world].get(a, 0) for a in aliases))
