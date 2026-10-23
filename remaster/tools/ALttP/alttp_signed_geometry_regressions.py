"""Run fixture-free dungeon regressions; account explicitly for fixture-only tests."""
import argparse
import json
from pathlib import Path
import subprocess

FIXTURE_ONLY = {
    'dungeon_cardinal_local_diagonal', 'dungeon_capture_height',
    'dungeon_diagonal_preview', 'dungeon_engine_scroll_route',
    'dungeon_placement_equivalence', 'dungeon_scroll_capture',
    'dungeon_stair_actual_state', 'dungeon_stair_emitter_sequence',
    'dungeon_variable_diagonal',
}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True, help='existing output directory')
    a = p.parse_args()
    if not a.output.is_dir():
        p.error('output must exist')
    records = []
    for source in sorted(Path('remaster/tests').glob('dungeon*test.cpp')):
        name = source.stem.removesuffix('_test')
        record = dict(source=str(source), name=name)
        if name in FIXTURE_ONLY:
            record['status'] = 'unsupported_external_fixture'
        else:
            binary = a.output / source.stem
            command = ['c++', '-std=c++17', '-O0', '-g', '-UNDEBUG', '-pthread', str(source), '-o', str(binary)]
            args = ['remaster/profiles/ALttP/alttp-profile.toml'] if name == 'dungeon_small_stack' else []
            record['compile_command'] = command
            compile = subprocess.run(command, capture_output=True, text=True, timeout=180)
            record['compile_exit'] = compile.returncode
            record['compile_output'] = compile.stdout + compile.stderr
            if compile.returncode:
                record['status'] = 'compile_failed'
            else:
                record['run_command'] = [str(binary), *args]
                run = subprocess.run(record['run_command'], capture_output=True, text=True, timeout=180)
                record.update(status='passed' if not run.returncode else 'failed',
                              run_exit=run.returncode, run_output=run.stdout+run.stderr)
        records.append(record)
        print(name, record['status'], flush=True)
    counts = {s: sum(r['status'] == s for r in records) for s in
              ('passed', 'failed', 'compile_failed', 'unsupported_external_fixture')}
    (a.output / 'regressions.json').write_text(json.dumps(dict(counts=counts, tests=records), indent=2)+'\n')
    print(counts)
    return int(bool(counts['failed'] or counts['compile_failed']))


if __name__ == '__main__':
    raise SystemExit(main())
