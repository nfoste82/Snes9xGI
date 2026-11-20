"""Generated BG2 bases for the editor and runtime; never edits authored TOML."""
import base64
import fcntl
import hashlib
import os
from pathlib import Path
import struct
import tempfile


WIDTH = 512
WORLD_BYTES = WIDTH * WIDTH * 3
CELL = struct.Struct('<Bh')  # absent, assigned, conflicting first arrival, unreached


class CalculatedHeightConflict(ValueError):
    pass


class CalculatedHeightStore:
    def __init__(self, annotations, weights):
        self.annotations = annotations
        self.weights = weights

    def _target(self, config):
        profile = config['binding']['profile_path']
        return Path(str(Path(profile).resolve()) + '.calculated-heights.bin') if profile else None

    def _read(self, path):
        header = b'ALTPCH1\n' + self.annotations.rom_sha256.encode('ascii')
        try:
            with path.open('rb') as source:
                raw = source.read(len(header) + 2 * WORLD_BYTES + 1)
        except FileNotFoundError:
            return header, None
        if (len(raw) != len(header) + 2 * WORLD_BYTES or not raw.startswith(header) or
                any(flag > 3 or (height == -32768 if flag in (1, 2) else height != 0)
                    for flag, height in CELL.iter_unpack(memoryview(raw)[len(header):]))):
            raise ValueError(f'Invalid or ROM-mismatched calculated height sidecar: {path}')
        return header, raw[len(header):]

    @staticmethod
    def _response(path, data):
        return dict(path=str(path) if path else None, revision=hashlib.sha256(data or b'').hexdigest(),
                    cells=base64.b64encode(data).decode('ascii') if data is not None else None)

    def config(self):
        self.annotations.refresh()
        profile = self.annotations.profile_path
        path = Path(str(profile.resolve()) + '.calculated-heights.bin') if profile else None
        return self._response(path, self._read(path)[1] if path else None)

    def save(self, request, expected):
        if not isinstance(request, dict):
            raise ValueError('Expected a calculated height area')
        world, bounds, cells = request['world'], request['bounds'], request['cells']
        x, y, w, h = (bounds[k] for k in ('x0', 'y0', 'width', 'height'))
        if (world not in ('light', 'dark') or any(type(v) is not int for v in (x, y, w, h)) or
                not (0 <= x < x + w <= WIDTH and 0 <= y < y + h <= WIDTH) or
                not isinstance(cells, list) or len(cells) != w*h):
            raise ValueError('Expected calculated cells matching bounds within a 512×512 world')
        area = bytearray(3*w*h)
        for i, cell in enumerate(cells):
            # null = reached authored cell (no independent calculated value).
            if cell is None:
                continue
            if (not isinstance(cell, list) or len(cell) != 2 or type(cell[0]) is not int or
                    type(cell[1]) is not int or not -32767 <= cell[1] <= 32767 or
                    (cell[0] not in (1, 2) and cell != [3, 0])):
                raise ValueError('Invalid calculated cell flag or signed integer height')
            CELL.pack_into(area, 3*i, *cell)
        config = self.weights.config()
        path = self._target(config)
        if not path:
            raise ValueError('Select a profile to persist calculated heights')
        if config['revision'] != request['input_revision']:
            raise CalculatedHeightConflict('Calculation inputs changed; recalculate before saving')
        with path.with_name(path.name + '.lock').open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            current = self.weights.config()
            if current['revision'] != request['input_revision'] or self._target(current) != path:
                raise CalculatedHeightConflict('Calculation inputs changed; recalculate before saving')
            header, old = self._read(path)
            if hashlib.sha256(old or b'').hexdigest() != expected:
                raise CalculatedHeightConflict('Calculated layer changed elsewhere; recalculate before saving')
            data = bytearray(old) if old is not None else bytearray(2 * WORLD_BYTES)
            for row in range(h):
                start = (WORLD_BYTES if world == 'dark' else 0) + 3*((y+row)*WIDTH + x)
                data[start:start+3*w] = area[3*row*w:3*(row+1)*w]
            temporary = None
            try:
                with tempfile.NamedTemporaryFile(dir=path.parent, prefix=path.name + '.', delete=False) as out:
                    temporary = Path(out.name)
                    out.write(header)
                    out.write(data)
                    out.flush()
                    os.fsync(out.fileno())
                os.replace(temporary, path)
            finally:
                if temporary:
                    temporary.unlink(missing_ok=True)
            return self._response(path, data)
