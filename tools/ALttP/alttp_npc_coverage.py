"""Conservative source inventory and observed-artwork audit, not reachability proof."""
from collections import Counter
import hashlib
import re
from pathlib import Path


def source_functions(text):
    """Extract C bodies with balanced braces after removing comments/strings."""
    clean = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', ' ', text, flags=re.S)
    result = {}
    for match in re.finditer(r'^\w[\w\s*]*?\b(\w+)\s*\([^;{}]*\)\s*\{', clean, re.M):
        depth, end = 1, match.end()
        while depth and end < len(clean):
            depth += (clean[end] == '{') - (clean[end] == '}')
            end += 1
        if depth:
            raise ValueError(f"Unbalanced source function {match[1]}")
        result[match[1]] = clean[match.end():end-1]
    return result


def integer_table(text, name):
    match = re.search(r'\b' + re.escape(name) + r'\s*\[[^]]*\]\s*=\s*\{([^}]+)\}', text, re.S)
    if not match:
        raise ValueError(f"Missing reference table {name}")
    return [int(x.strip(), 0) for x in match[1].split(',') if x.strip()]


def link_sequences(engine):
    """Bounded states supported by inspected player update and OAM branches.

    Each sequence records its evidence. Checks intentionally fail on reference
    changes instead of silently inventing new frame limits.
    """
    player = (engine / 'src/player.c').read_text()
    oam = (engine / 'src/player_oam.c').read_text()
    functions = source_functions(player)
    if not re.search(r'\+\+link_animation_steps\s*==\s*9', functions['Link_HandleMovingAnimation_StartWithDash']):
        raise ValueError('Re-audit Link movement bounds in reference player.c')
    spin = integer_table(player, 'kLinkSpinGraphicsByDir')
    if len(spin) != 48 or max(spin) != 15:
        raise ValueError('Re-audit Link spin sequence')
    adapter = Path(__file__).with_name('alttp_npc_pose_adapter.c').read_text()
    if integer_table(adapter, 'spin') != spin:
        raise ValueError('Link adapter spin sequence does not match reference player.c')
    offsets = integer_table(oam, 'kPlayerOamOtherOffs')
    if len(offsets) != 160:
        raise ValueError('Re-audit Link OAM action table')
    definitions = [
        ('walking', 9, 0, 'Link_HandleMovingAnimation_StartWithDash / Link_HandleMovingAnimation_Dash'),
        ('sword swing', 9, 1, 'LinkItem_Sword: button_b_frames 0..8'),
        ('sword charge / charged movement', 6, 2, 'Player_Sword_SpinAttackJerks_HoldDown: button_b_frames=9; six-step movement'),
        ('sword wall tap', 3, 3, 'Player_Sword_SpinAttackJerks_HoldDown: button_b_frames 10..12'),
        ('spin attack / charged release', 12, 4, 'LinkState_SpinAttack / kLinkSpinGraphicsByDir: 12 chronological steps per facing'),
        ('bunny movement', 4, 5, 'LinkOam_Main: link_is_bunny_mirror; animation & 3'),
        ('swimming idle', 2, 6, 'LinkOam_Main: Swimming; animation & 1'),
        ('recoil', 1, 7, 'LinkOam_Main: link_auxiliary_state=1; action 5, step 0'),
        ('electrocution', 4, 8, 'LinkOam_Main: link_electrocute_on_touch; player_handler_timer & 3'),
        ('item held overhead (one hand)', 1, 9, 'LinkOam_Main: link_pose_for_item=1; action 0x1d'),
        ('item held overhead (two hands)', 1, 10, 'LinkOam_Main: link_pose_for_item=2; action 0x1e'),
        ('sword held overhead', 1, 11, 'LinkOam_Main: link_force_hold_sword_up; action 0x24'),
    ]
    return [dict(action=action, steps=steps, adapter_state=state, evidence=evidence,
                 directions=4, status='modeled_unverified')
            for action, steps, state, evidence in definitions]


def build_coverage(engine, catalog, actors):
    files = {p.name: p.read_text() for p in sorted((engine / 'src').glob('*.c'))}
    sprite = files['sprite_main.c']
    match = re.search(r'kSpriteActiveRoutines\[243\]\s*=\s*\{(.*?)\};', sprite, re.S)
    if not match:
        raise ValueError('Reference sprite dispatch table missing')
    table = re.sub(r'/\*.*?\*/|//[^\n]*', '', match[1], flags=re.S)
    entries = [x.strip().lstrip('&') for x in table.split(',') if x.strip()]
    if len(entries) != 243:
        raise ValueError('Reference sprite dispatch count changed')
    functions = {}
    for filename, text in files.items():
        for name, body in source_functions(text).items():
            functions.setdefault(name, (filename, body))
    by_type = {}
    for model in catalog:
        by_type.setdefault(model['type_id'], []).append(model['key'])
    generated = Counter(a['type_id'] for a in actors if a['draw_model']['draw'] != 'link')
    types = []
    for type_id, routine in enumerate(entries):
        source, body = functions.get(routine, ('sprite_main.c', ''))
        calls = sorted(set(re.findall(r'\b(\w+)\s*\(', body)) & functions.keys())
        # Static call reachability is a work queue, not proof that any particular
        # branch executes. Include shared drawing helpers rather than assuming
        # the dispatcher itself contains all animation selection.
        pending, reachable = [routine], set()
        while pending:
            callee = pending.pop()
            if callee in reachable or callee not in functions:
                continue
            reachable.add(callee)
            pending.extend((set(re.findall(r'\b(\w+)\s*\(', functions[callee][1])) & functions.keys()) - reachable)
        draw_routines = sorted(r for r in reachable if 'Draw' in r or 'Animation' in r)
        types.append(dict(type_id=type_id, routine=routine, source=source,
                          direct_calls=calls,
                          reachable_draw_candidates=draw_routines,
                          draw_state_inputs=sorted(set(re.findall(r'\bsprite_\w+', body))),
                          models=[key for key in by_type.get(type_id, []) if key != 'link'],
                          generated_contexts=generated[type_id],
                          status='no_dispatch' if routine == 'NULL' else 'partial' if generated[type_id] else 'unmodeled',
                          gap='Draw branches, valid state vectors, dynamic spawns and runtime assembly parity require review.'))
    families = []
    for name in sorted({a['name'] for a in actors}):
        contexts = [a for a in actors if a['name'] == name]
        families.append(dict(name=name, status='partial', contexts=len(contexts),
                             poses=sum(len(a['poses']) for a in contexts),
                             actions=sorted({p['action'] for a in contexts for p in a['poses']}),
                             gaps=contexts[0]['coverage']['gaps']))
    return dict(format='alttp-npc-coverage-v1', complete=False,
                boundary='Source inventory and bounded adapters; no exhaustive draw-branch or gameplay reachability claim.',
                sprite_dispatch_count=len(types), sprite_types=types, families=families,
                source_sha256={name: hashlib.sha256(files[name].encode()).hexdigest()
                               for name in ('sprite_main.c', 'player.c', 'player_oam.c', 'nmi.c')},
                other_actor_systems=['player', 'followers', 'ancilla/projectiles', 'garnish/effects',
                                     'overlords/dynamic spawners', 'background-composited actors', 'cutscenes'])


def compare_observations(manifest, observations):
    known = {p['tile_hash'] for a in manifest['actors'] for pose in a['poses'] for p in pose['parts']}
    results = []
    for capture in observations:
        tiles = capture['tiles']
        results.append(dict(capture=capture['capture'], observed_tiles=len(tiles),
                            absent_from_library=[t for t in tiles if t['tile_hash'] not in known],
                            missing_height=[t['tile_hash'] for t in tiles if not t['has_height']],
                            missing_normals=[t['tile_hash'] for t in tiles if not t['has_normals']],
                            boundary='OBJ artwork presence only. Capture animation variants are candidates, not actor/state or assembled-pose parity proof.'))
    return dict(format='alttp-npc-observation-audit-v1', complete=False, captures=results)
