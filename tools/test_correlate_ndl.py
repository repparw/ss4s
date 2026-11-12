import unittest
from correlate_ndl import correlate

STAMP = '2026-11-11T12:00:01Z '
KEY = 'session=1 generation=1 window=0 '
SUMMARY = STAMP + 'NDL_PRESENT_V1 ' + KEY + 'start_us=0 end_us=1000000 emitted_us=1000000 attempts=1 accepted=1 failed=0 not_ready=0'
METRIC = STAMP + 'NDL_PRESENT_V1 ' + KEY + 'metric={} n=1 sum=1 min=1 max=1 buckets=1,0,0,0,0,0,0,0,0,0,0,0'
FULL = SUMMARY + '\n' + '\n'.join(METRIC.format(name) for name in ['pts_ms', 'accepted_pts_ms', 'play_us', 'feed_us'])
ML = STAMP + 'ML_BENCH_V1 duration_ms=1000 received=120 submitted=120'


def parse(line):
    return {k: int(v) for k, v in (token.split('=') for token in line.split()[1:])}


class CorrelationTests(unittest.TestCase):
    def test_overlap(self):
        row = correlate(FULL, ML, parse)['windows'][0]
        self.assertEqual(row['missing_components'], [])
        self.assertEqual(row['ml_intersections'][0]['overlap_s'], 1)
        self.assertEqual(row['ml_intersections'][0]['submitted'], 120)

    def test_delayed_flush(self):
        row = correlate(FULL.replace('emitted_us=1000000', 'emitted_us=2000000'), ML, parse)['windows'][0]
        self.assertEqual(row['ml_intersections'], [])

    def test_missing_is_not_zero(self):
        row = correlate(SUMMARY, ML, parse)['windows'][0]
        self.assertEqual(len(row['missing_components']), 4)
        self.assertIsNone(row['missing_windows_before'])

    def test_empty(self):
        with self.assertRaisesRegex(ValueError, 'missing'):
            correlate('', ML, parse)

    def test_duplicate(self):
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            correlate(FULL + '\n' + SUMMARY, ML, parse)

    def test_missing_window(self):
        rows = correlate(FULL + '\n' + FULL.replace('window=0', 'window=2'), ML, parse)['windows']
        self.assertEqual(rows[1]['missing_windows_before'], 1)

    def test_timestamp_required(self):
        with self.assertRaisesRegex(ValueError, 'timestamp'):
            correlate(SUMMARY.replace(STAMP, ''), ML, parse)

    def test_malformed_histogram(self):
        with self.assertRaisesRegex(ValueError, 'histogram'):
            correlate(FULL.replace('n=1', 'n=2'), ML, parse)


if __name__ == '__main__':
    unittest.main()
