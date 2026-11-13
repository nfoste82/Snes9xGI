"""Experimental editor-only weights. Never writes semantic/profile geometry."""
from __future__ import annotations

import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import tempfile
import threading

FORMAT = 'alttp-experimental-height-weights-v2'
LEGACY_FORMAT = 'alttp-experimental-height-weights-v1'
HALF_FAMILIES = ('437727f0595cb424', 'a8ec34c0c0e99095', 'bcd3a49452020d15')


def validate_weight(value):
    if (isinstance(value, bool) or not isinstance(value, (int, float)) or
            not math.isfinite(value) or not 0 <= value <= 32 or value != int(value)):
        raise ValueError('weight must be an integer from 0 to 32 (0 disables its delta)')
    return int(value)


class WeightConflict(ValueError):
    pass


class HeightWeightStore:
    def __init__(self, annotations, catalog, path: Path | None = None):
        self.annotations = annotations
        self.catalog = catalog
        self.override = path.resolve() if path else None
        self.lock = threading.Lock()

    def _read(self):
        semantic_revision = self.annotations.refresh()
        profile = str(self.annotations.profile_path.resolve()) if self.annotations.profile_path else None
        base = Path(profile) if profile else self.annotations.path.resolve()
        path = self.override or base.with_name(base.name + '.height-weights.json')
        binding = dict(rom_sha256=self.annotations.rom_sha256, profile_path=profile,
                       annotation_path=str(self.annotations.path.resolve()) if not profile else None)
        raw = path.read_bytes() if path.exists() else b''
        overrides = {}
        if raw:
            doc = json.loads(raw)
            if doc.get('format') not in (FORMAT, LEGACY_FORMAT) or doc.get('binding') != binding:
                raise WeightConflict('weight sidecar ROM/profile binding differs; choose the correct --height-weights file')
            if not isinstance(doc.get('weights'), dict):
                raise ValueError('weights must be an object')
            for key, weight in doc['weights'].items():
                if not self.catalog.contains(key):
                    raise ValueError(f'unknown weight tile: {key}')
                canonical = min(self.catalog.semantic_group(key))
                if doc['format'] == LEGACY_FORMAT:
                    if (isinstance(weight, bool) or not isinstance(weight, (int, float)) or
                            not math.isfinite(weight) or not 0 <= weight <= 4):
                        raise ValueError('invalid legacy weight')
                    weight *= 8
                weight = validate_weight(weight)
                if canonical in overrides and overrides[canonical] != weight:
                    raise ValueError('conflicting alias weights')
                overrides[canonical] = weight
        effective = {}
        for key in self.catalog.aliases:
            aliases = self.catalog.semantic_group(key)
            default = 4 if any(a.endswith(':' + h) for a in aliases for h in HALF_FAMILIES) else 8
            effective[key] = overrides.get(min(aliases), default)
        revision = hashlib.sha256(raw + json.dumps(binding, sort_keys=True).encode() +
                                  semantic_revision.encode()).hexdigest()
        return dict(format=FORMAT, binding=binding, path=str(path), revision=revision,
                    file_revision=hashlib.sha256(raw).hexdigest(),
                    semantic_revision=semantic_revision, weights=effective), overrides

    def config(self):
        with self.lock:
            return self._read()[0]

    def save(self, content_id, weight, revision):
        weight = validate_weight(weight)
        if not self.catalog.contains(content_id):
            raise ValueError('unknown background tile')
        with self.lock:
            config, _ = self._read()
            path = Path(config['path'])
            path.parent.mkdir(parents=True, exist_ok=True)
            # Separate lock inode survives atomic replacement; protects multiple servers.
            with path.with_name(path.name + '.lock').open('a') as lock:
                fcntl.flock(lock, fcntl.LOCK_EX)
                current, overrides = self._read()
                if current['revision'] != revision or current['path'] != str(path):
                    raise WeightConflict('weights, semantics or selected profile changed; reload before saving')
                overrides[min(self.catalog.semantic_group(content_id))] = weight
                doc = dict(format=FORMAT, binding=current['binding'], weights=overrides)
                temporary = None
                try:
                    with tempfile.NamedTemporaryFile(mode='w', dir=path.parent,
                                                     prefix=path.name + '.', suffix='.tmp', delete=False) as out:
                        temporary = Path(out.name)
                        json.dump(doc, out, indent=2, sort_keys=True, allow_nan=False)
                        out.write('\n')
                        out.flush()
                        os.fsync(out.fileno())
                    # Marker/semantic changes during serialization must not retarget a write.
                    check, _ = self._read()
                    if check['revision'] != revision or check['path'] != str(path):
                        raise WeightConflict('selected profile or metadata changed; reload before saving')
                    os.replace(temporary, path)
                finally:
                    if temporary and temporary.exists():
                        temporary.unlink()
                return self._read()[0]
