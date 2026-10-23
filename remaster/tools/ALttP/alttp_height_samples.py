"""Authored local-height contracts (placement bases are deliberately separate)."""
import struct


def valid_heights(values, schema_version=27):
    lo, hi = (-32768, 32767) if schema_version >= 27 else (0, 255)
    return (isinstance(values, (list, tuple)) and len(values) == 64 and
            all(type(v) is int and lo <= v <= hi for v in values))


def height_bytes(values):
    """Canonical signed little-endian samples for offline evidence digests."""
    if not values:
        return b''
    if not valid_heights(values):
        raise ValueError('invalid signed int16 height samples')
    return struct.pack('<64h', *values)
