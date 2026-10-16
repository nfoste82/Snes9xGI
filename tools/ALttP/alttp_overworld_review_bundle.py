#!/usr/bin/env python3
"""Build the local castle review bundle from available original Snes9x captures."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from alttp_overworld_replay import generate, ROOT
from alttp_overworld_review import generate as generate_html


def build(candidate, review, output, producer, captures, app, standing_fixture=None,
          artwork=None, capture_candidates=()):
    output.mkdir(parents=True, exist_ok=True)
    selected = {}
    for path in capture_candidates:
        pin = json.loads(path.read_text())['pin']
        digest = pin.get('capture_sha256')
        if not digest or digest in selected:
            raise ValueError('capture candidates require unique source capture SHA256 pins')
        selected[digest] = path
    revision = json.loads(candidate.read_text())['pin']['metadata_revision']
    if artwork is None:
        raise ValueError('specify artwork matching this candidate revision')
    # Verify overview pins/digest before replacing any replay or manifest.
    generate_html(candidate, review, output/'index.html', artwork)
    entries = []
    rejected = []
    for capture in captures:
        target = output/(capture.stem+'-proposed.s9xrmf')
        try:
            attestation = None
            if standing_fixture is not None:
                pinned = json.loads(standing_fixture.read_text())
                if hashlib.sha256(capture.read_bytes()).hexdigest() == pinned['capture_sha256']:
                    attestation = standing_fixture
            source_candidate = selected.get(hashlib.sha256(capture.read_bytes()).hexdigest(), candidate)
            report = generate(source_candidate, capture, target, producer, attestation)
        except subprocess.CalledProcessError as exc:
            rejected.append({'capture': str(capture), 'reason': exc.stderr.strip()})
            continue
        entries.append(dict(report, title=capture.stem, path=target.name))
    manifest = dict(format='S9XAREA-REVIEW1', title=f'Castle exterior — {revision}',
                    status=f'{revision}: incomplete area; reviewed wall-height repairs',
                    html='index.html', path='.', captures=entries, rejected=rejected)
    manifest_path = output/'area-review.json'
    manifest_path.write_text(json.dumps(manifest, indent=2)+'\n')
    # POSIX single-quote encoding makes checkout paths with spaces/apostrophes safe.
    quote = lambda p: "'"+str(p).replace("'", "'\\''")+"'"
    launcher = output/'Open Castle Review.command'
    launcher.write_text('#!/bin/sh\nexec /usr/bin/open -a '+quote(app.resolve())+' '+quote(manifest_path.resolve())+'\n')
    launcher.chmod(0o755)
    (output/'README.md').write_text(
        '# Castle geometry review\n\nDouble-click **Open Castle Review.command**. '
        'The native panel offers capture navigation and the complete-area overview.\n\n'
        'In Scene Controls select Height, Normals, Occlusion, Visibility, or lighting diagnostics. '
        'Use the overview to inspect placement evidence and record/export issues. '
        'The overview artwork is diagnostic grayscale, not captured game color.\n\n'
        'This is an incomplete proposal, not a ready/approved area. Unsupported geometry '
        'and map/graphics mismatches remain explicit. Occlusion has no newly authored '
        'coverage; use geometry visibility/shadows to inspect the generated blocking. '
        'See area-review.json and each packet’s .review.json for exact counts.\n')
    return manifest


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    base = ROOT/'build/remaster-geometry'
    p.add_argument('--candidate', type=Path, default=base/'overworld-castle-authored-v1.json')
    p.add_argument('--review', type=Path, default=base/'overworld-castle-authored-v1-review.json')
    p.add_argument('--output', type=Path, default=base/'overworld-castle-review')
    p.add_argument('--producer', type=Path, required=True)
    p.add_argument('--app', type=Path, default=ROOT/'build/debug/Snes9x.app')
    p.add_argument('--standing-fixture', type=Path)
    p.add_argument('--artwork', type=Path, required=True)
    p.add_argument('--capture-candidate', type=Path, action='append', default=[])
    p.add_argument('captures', type=Path, nargs='+')
    a = p.parse_args()
    result = build(a.candidate, a.review, a.output, a.producer, a.captures, a.app,
                   a.standing_fixture, a.artwork, a.capture_candidate)
    print(json.dumps({'captures': len(result['captures']), 'rejected': result['rejected'],
                      'manifest': str(a.output/'area-review.json')}, indent=2))
