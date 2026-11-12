"""Join NDL windows to timestamped ML_BENCH_V1 windows; never infer presentation."""
import argparse
from datetime import datetime
import importlib.util
import json
from pathlib import Path
import re


def rows(text, marker):
    for line in text.splitlines():
        if marker not in line:
            continue
        stamp = re.search(r'\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(?:\.\d+)?(?:Z|[+-]\d\d:\d\d)', line)
        if not stamp:
            raise ValueError(f'{marker}: ISO timestamp required')
        fields = {}
        for token in line.split(marker, 1)[1].split():
            key, value = token.split('=', 1)
            if key in fields:
                raise ValueError(f'duplicate {key}')
            fields[key] = value
        yield datetime.fromisoformat(stamp[0].replace('Z', '+00:00')).timestamp(), fields


def correlate(ndl_text, moonlight_text, parse_ml):
    ml = []
    for stamp, fields in rows(moonlight_text, 'ML_BENCH_V1'):
        row = parse_ml('ML_BENCH_V1 ' + ' '.join(f'{k}={v}' for k, v in fields.items()))
        ml.append((stamp - row['duration_ms'] / 1000, stamp, row))
    groups = {}
    for stamp, row in rows(ndl_text, 'NDL_PRESENT_V1'):
        key = tuple(int(row[k]) for k in ('session', 'generation', 'window'))
        name = row.get('metric', 'summary')
        group = groups.setdefault(key, {})
        if name in group:
            raise ValueError(f'duplicate window component: {key} {name}')
        group[name] = (stamp, row)
    if not groups or not ml:
        raise ValueError('missing NDL or Moonlight samples')
    output = []
    previous = {}
    for key, group in sorted(groups.items()):
        missing = sorted({'summary', 'pts_ms', 'accepted_pts_ms', 'play_us', 'feed_us'} - group.keys())
        item = dict(session=key[0], generation=key[1], window=key[2], missing_components=missing)
        prior = previous.get(key[:2], -1)
        item['missing_windows_before'] = max(0, key[2] - prior - 1) if prior >= 0 else None
        previous[key[:2]] = key[2]
        if 'summary' not in group:
            output.append(item)
            continue
        stamp, summary = group['summary']
        start_us, end_us = int(summary['start_us']), int(summary['end_us'])
        counts = {k: int(summary[k]) for k in ('attempts', 'accepted', 'failed', 'not_ready')}
        if end_us < start_us or min(counts.values()) < 0 or counts['attempts'] != sum(counts[k] for k in ('accepted', 'failed', 'not_ready')):
            raise ValueError('invalid NDL window')
        for name, (_, metric) in group.items():
            if name == 'summary':
                continue
            if name not in {'pts_ms', 'accepted_pts_ms', 'play_us', 'feed_us'}:
                raise ValueError('unknown metric')
            buckets = [int(v) for v in metric['buckets'].split(',')]
            if len(buckets) != 12 or min(buckets) < 0 or sum(buckets) != int(metric['n']):
                raise ValueError('invalid histogram')
            count = int(metric['n'])
            expected = counts['attempts'] if name == 'feed_us' else counts['attempts'] - counts['not_ready']
            if (name in {'feed_us', 'play_us'} and count != expected) or count > expected:
                raise ValueError('histogram sample count disagrees with summary')
        emitted_us = int(summary['emitted_us'])
        if emitted_us < end_us:
            raise ValueError('emission precedes window end')
        stamp -= (emitted_us - end_us) / 1e6
        begin = stamp - (end_us - start_us) / 1e6
        intersections = [dict(overlap_s=min(stamp, end) - max(begin, start), **row)
                         for start, end, row in ml if min(stamp, end) > max(begin, start)]
        item.update(counts=counts, start_utc=begin, end_utc=stamp, ml_intersections=intersections,
                    distributions={name: row for name, (_, row) in group.items() if name != 'summary'})
        output.append(item)
    return {'windows': output, 'limit': 'UTC emission times approximate window ends. Intersecting ML counts are whole source windows; do not sum repeated intersections. Missing components/windows stay unknown. No display-frame count is measured.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndl', type=Path, required=True)
    parser.add_argument('--moonlight', type=Path, required=True)
    parser.add_argument('--moonlight-benchmark-dir', type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('moonlight_compare', args.moonlight_benchmark_dir / 'compare.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    print(json.dumps(correlate(args.ndl.read_text(), args.moonlight.read_text(), module.parse_line), indent=2))
