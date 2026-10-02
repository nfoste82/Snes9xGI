#!/usr/bin/env python3
"""Summarize benchmark reports or exported xctrace CPU/Metal XML (stdlib only)."""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import statistics
import xml.etree.ElementTree as ET


def median(values):
    return statistics.median(values) if values else 0


def report(path):
    data = json.loads(path.read_text())
    print(f"\n{path.parent.name}: counters disabled={data.get('stage_counters_disabled', 'unknown')}")
    for case in data['cases']:
        samples = case['samples']
        def values(key):
            return [s.get(key, 0) for s in samples]
        sums = [sum(s.get('gpu_stages_ms', {}).values()) for s in samples]
        gaps = [s['gpu_ms'] - total for s, total in zip(samples, sums) if s.get('gpu_stages_ms')]
        direct_shares = [s['gpu_stages_ms'].get('direct_transport', 0) / total * 100
                         for s, total in zip(samples, sums) if total > 0]
        print(f"  {case['name']}: n={len(samples)} gpu={median(values('gpu_ms')):.3f} "
              f"fields={median(values('scene_fields_ms')):.3f} setup={median(values('gpu_setup_ms')):.3f} "
              f"queue={median(values('queue_ms')):.3f} drawable={median(values('drawable_ms')):.3f} "
              f"submit->gpu={median(values('submit_to_gpu_ms')):.3f} driver={median(values('driver_ms')):.3f} "
              f"stage-gap={median(gaps):.3f} direct-share={median(direct_shares):.1f}% "
              f"emitters={min(values('emitters'))}..{max(values('emitters'))} "
               f"samples={min(values('direct_samples'))}..{max(values('direct_samples'))}")
        phases = defaultdict(list)
        transitions, steady = [], []
        previous = None
        for sample in samples:
            phase = sample.get('emitter_signature', (sample.get('emitters'), sample.get('direct_samples')))
            phases[phase].append(sample)
            if previous is not None:
                (transitions if phase != previous else steady).append(sample)
            previous = phase
        if len(phases) <= 16:
            for phase, entries in phases.items():
                print(f"    phase={phase}: n={len(entries)} "
                      f"emitters/sources={entries[0].get('emitters')}/{entries[0].get('direct_samples')} "
                      f"gpu={median([s['gpu_ms'] for s in entries]):.3f} "
                      f"source-build={median([s.get('source_build_ms', 0) for s in entries]):.3f} ms")
        else:
            print(f"    {len(phases)} distinct source signatures (movement/geometry can change these too)")
        for label, entries in [('phase-change', transitions), ('phase-held', steady)]:
            print(f"    {label}: n={len(entries)} gpu={median([s['gpu_ms'] for s in entries]):.3f} "
                  f"source-build={median([s.get('source_build_ms', 0) for s in entries]):.3f} ms")


def trace(path):
    tree = ET.parse(path)
    ids = {e.attrib['id']: e for e in tree.iter() if 'id' in e.attrib}
    def resolve(e):
        return ids[e.attrib['ref']] if e is not None and 'ref' in e.attrib else e
    schema = tree.find('.//schema')
    columns = [c.findtext('mnemonic') for c in schema.findall('col')]
    rows = tree.findall('.//row')
    def cells(row):
        return {key: resolve(e) for key, e in zip(columns, row)}
    print(f"\n{path.name}: schema={schema.attrib['name']} rows={len(rows)}")
    if schema.attrib['name'] == 'time-profile':
        inclusive, leaf, threads = Counter(), Counter(), Counter()
        total = 0
        for row in rows:
            c = cells(row)
            weight = float(c['weight'].text) / 1e6
            frames = [resolve(f).attrib.get('name', '?') for f in c['stack'].findall('frame')]
            total += weight
            threads[c['thread'].attrib.get('fmt', '?')] += weight
            if frames:
                leaf[frames[0]] += weight
            for name in set(frames):
                inclusive[name] += weight
        print(f"  Sampled running CPU weight: {total:.0f} ms (not frame time; excludes waits)")
        for title, counts in [('threads', threads), ('inclusive symbols', inclusive), ('leaf symbols', leaf)]:
            print(f"  {title}:")
            for name, weight in counts.most_common(18):
                print(f"    {weight:8.1f} ms {weight / total * 100:5.1f}% {name}")
        print('  Targeted inclusive CPU weights (overlap; not additive):')
        for prefix in ['RemasterSurfaceMesh::Cache::update', 'S9xRemasterApplyProfileToFrame',
                       'S9xRemasterFinalizeFrame', 'S9xPutImageMetal', 'DrawRemasterFrame']:
            weight = sum(v for k, v in inclusive.items() if k.startswith(prefix))
            print(f"    {weight:8.1f} ms {weight / total * 100:5.1f}% {prefix}")
    elif schema.attrib['name'] == 'metal-gpu-intervals':
        groups, processes = defaultdict(list), Counter()
        for row in rows:
            c = cells(row)
            process = c['process'].attrib.get('fmt', 'unknown')
            duration = float(c['duration'].text) / 1e6
            processes[process] += duration
            if 'Snes9x' not in process:
                continue
            label = c['event-label'].attrib.get('fmt', '?').split('      ')[0]
            start = float(c['start'].text) / 1e6
            groups[(label, c['encoder-id'].text)].append((start, start + duration))
        print('  Process interval-duration sums (overlap possible; not utilization):')
        for name, duration in processes.most_common():
            print(f"    {duration:9.1f} ms {name}")
        labels = defaultdict(list)
        for (label, encoder), spans in groups.items():
            active = sum(end - start for start, end in spans)
            elapsed = max(end for _, end in spans) - min(start for start, _ in spans)
            labels[label].append((active, elapsed, len(spans)))
        print('  Snes9x encoder intervals: active-sum / elapsed-span medians; split count')
        for label, entries in labels.items():
            print(f"    n={len(entries):3} {median([e[0] for e in entries]):8.3f} / "
                  f"{median([e[1] for e in entries]):8.3f} ms "
                  f"splits={median([e[2] for e in entries]):g} {label}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inputs', nargs='+', type=Path)
    args = parser.parse_args()
    for path in args.inputs:
        if path.is_dir():
            path /= 'report.json'
        (trace if path.suffix == '.xml' else report)(path)


if __name__ == '__main__':
    main()
