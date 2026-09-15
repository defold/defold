# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0

import unittest

from ci.dotnet_cache_timing import cache_status, format_report


class DotnetCacheTimingTests(unittest.TestCase):
    # An empty cache-hit output is a miss only when the cache action actually ran successfully.
    def test_cache_outcomes_distinguish_misses_from_skips_and_failures(self):
        for outcome, hit, expected in (
                ('success', 'true', 'hit'), ('success', 'false', 'miss'),
                ('success', '', 'miss'), ('skipped', '', 'skipped'),
                ('failure', '', 'failure'), ('', '', 'unavailable')):
            with self.subTest(outcome=outcome, hit=hit):
                self.assertEqual(expected, cache_status(outcome, hit))

    # Warm-run comparisons must include restore time and visibly retain failed setup results.
    def test_report_includes_restore_setup_and_combined_time(self):
        report = format_report({'started_ns': 1_000_000_000, 'setup_started_ns': 4_000_000_000},
                               9_500_000_000, {'DOTNET_CACHE_OUTCOME': 'success',
                                             'DOTNET_CACHE_HIT': 'true',
                                             'DOTNET_SETUP_OUTCOME': 'failure',
                                             'DOTNET_SDK_VERSION': '9.0.121'})
        self.assertIn('Cache result | hit', report)
        self.assertIn('Setup result | failure', report)
        self.assertIn('lookup / restore | 3.00s', report)
        self.assertIn('SDK setup | 5.50s', report)
        self.assertIn('Total before build | 8.50s', report)
        self.assertIn('Post-job cache saving is excluded', report)


if __name__ == '__main__':
    unittest.main()
