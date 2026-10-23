"""Read the reference engine's graphics slot context, without inferring actors."""
import re


def special_overworld_contexts(engine):
    text = (engine / 'src/overworld.c').read_text()
    tables = []
    for name in ('kSpExit_SprGfx', 'kSpExit_PalSpr'):
        match = re.search(rf'{name}\[16\]\s*=\s*\{{([^}}]+)\}}', text)
        if not match:
            raise ValueError(f'reference special-overworld table changed: {name}')
        tables.append([int(value.strip(), 0) for value in match[1].split(',') if value.strip()])
    # Special screen IDs are not special-exit indices. In particular screen
    # 0x81 (Zora's Domain) is entered through exit 0x182, not 0x181.
    screens = re.search(r'kSpecialSwitchAreaB_Screen\[3\]\s*=\s*\{([^}]+)\}', text)
    exits = re.search(r'kSpecialSwitchArea_Exit\[4\]\s*=\s*\{([^}]+)\}', text)
    if not screens or not exits:
        raise ValueError('reference special-overworld screen/exit mapping changed')
    screen_ids = [int(v.strip(), 0) for v in screens[1].split(',') if v.strip()]
    exit_ids = [int(v.strip(), 0) for v in exits[1].split(',') if v.strip()]
    result = {}
    for screen, exit_id in zip(screen_ids, exit_ids):
        result.setdefault(screen, (tables[0][exit_id - 0x180], tables[1][exit_id - 0x180]))
    return result


def sprite_tilesets(engine):
    text = (engine / 'src/load_gfx.c').read_text()
    match = re.search(r'kSpriteTilesets\[144\]\[4\]\s*=\s*\{(.*?)\n\};', text, re.S)
    if not match:
        raise ValueError('reference sprite tileset table changed')
    rows = [tuple(map(int, re.findall(r'\d+', row)))
            for row in re.findall(r'\{([^{}]+)\}', match[1])]
    if len(rows) != 144 or any(len(row) != 4 for row in rows):
        raise ValueError('invalid reference sprite tileset table')
    return rows
