"""Versioned doorway semantics; never infer closed door leaves or support direction."""
from copy import deepcopy


def migrate_profile(profile):
    result = deepcopy(profile)
    legacy = result.get('schema_version', 0) < 34
    for asset in result.get('assets', []):
        if legacy and asset.get('tile_role') == 'door':
            asset['tile_role'] = 'doorway'
        elif legacy and asset.get('tile_role') == 'doorway':
            raise ValueError('doorway role requires schema_version 34')
        if legacy and ('doorway_support' in asset or 'doorway_support_direction' in asset):
            raise ValueError('doorway support requires schema_version 34')
        validate_asset(asset)
    return result


def validate_asset(asset):
    support = asset.get('doorway_support', False)
    direction = asset.get('doorway_support_direction')
    if not isinstance(support, bool):
        raise ValueError('doorway_support must be boolean')
    if support and (asset.get('tile_role') != 'doorway' or direction not in ('north', 'south', 'east', 'west')):
        raise ValueError('doorway support requires doorway role and cardinal direction')
    if not support and direction is not None:
        raise ValueError('support direction requires doorway_support = true')
    if 'doorframe_mask' in asset and asset.get('tile_role') != 'doorway':
        raise ValueError('doorframe_mask requires doorway role')


def transformed_direction(direction, hflip=False, vflip=False):
    if hflip:
        direction = {'east': 'west', 'west': 'east'}.get(direction, direction)
    if vflip:
        direction = {'north': 'south', 'south': 'north'}.get(direction, direction)
    return direction
