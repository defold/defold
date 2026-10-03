"""Regression checks for mixed worker/roundtrip records and frame-age clocks."""
import csv
import io
import tempfile
import unittest
from pathlib import Path
from analysis import analyze


class AnalysisTest(unittest.TestCase):
    def calculate(self, second_input=19999):
        data = {
            'key': 'test', 'case': {'name': 'fixture'}, 'mode': 'cached_retry', 'repeat': 1,
            'result': {'measurement_clock_start': 0, 'elapsed_seconds': .040, 'update_intervals_per_second': 100},
            'memory': [
                {'heap': 1048576, 'rss': 1048576, 'now': 0, 'processes': [{'id': 1, 'cpuTime': 0}]},
                {'heap': 1048576, 'rss': 1048576, 'now': 40, 'processes': [{'id': 1, 'cpuTime': .020}]},
            ],
            'schedule': {
                'worker': [[3, 0, 10, 12], [2, 0, 10, 14], [2, 0, 20, 24]],
                'main': [[1, 0, 9, 9.1], [1, 0, 19, 19.1]], 'retries': 1,
            },
        }
        rows = []
        for i, start in enumerate([11999, second_input]):
            rows.append({'input_begin_us': start, 'update_end_us': 13000+i*10000,
                         'prepare_end_us': 13500+i*10000, 'queue_drain_end_us': 14000+i*10000,
                         'render_begin_us': 16000+i*10000, 'submit_end_us': 18000+i*10000})
        trace = io.StringIO()
        trace.write('# dropped=0;\n')
        writer = csv.DictWriter(trace, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'test.csv').write_text(trace.getvalue())
            return analyze(data, root)

    # Verifies nested graphics calls are not counted as updates and dispatch age
    # includes pre-simulation time, including a 1 us cross-clock rounding edge.
    def test_dispatch_age_and_nested_roundtrip(self):
        row = self.calculate()
        self.assertAlmostEqual(9, row['dispatch_age'])
        self.assertAlmostEqual(7.001, row['age'])
        self.assertAlmostEqual(4, row['worker'])
        self.assertAlmostEqual(1, row['sync_ms'])
        self.assertAlmostEqual(.5, row['sync_calls'])
        self.assertAlmostEqual(5, row['idle'])

    # Verifies an unmatched clock/frame record fails rather than producing a
    # plausible-looking latency from the wrong worker update.
    def test_reject_unmatched_frame_clock(self):
        with self.assertRaisesRegex(AssertionError, 'Frame does not match'):
            self.calculate(second_input=15000)


if __name__ == '__main__':
    unittest.main()
