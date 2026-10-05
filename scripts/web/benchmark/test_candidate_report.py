"""Regression checks for candidate acceptance and missing-evidence handling."""
import unittest
from candidate_report import evaluate, analyze_run


class CandidateReportTest(unittest.TestCase):
    # A forced-collection run must never silently enter production timing gates.
    def test_gc_diagnostic_is_rejected(self):
        with self.assertRaisesRegex(AssertionError, "GC diagnostic"):
            analyze_run({"memoryDiagnostic": True})

    # Per-update instrumentation must remain separate from acceptance timings.
    def test_update_diagnostic_is_rejected(self):
        with self.assertRaisesRegex(AssertionError, "Update diagnostic"):
            analyze_run({"diagnostics": {"schema": 1}})

    def setUp(self):
        self.gates = dict(control_mode='direct', candidate_mode='candidate', minimum_repeats=3,
                          minimum_measurement_seconds=120, target_workloads=['heavy'],
                          target_throughput_gain_percent=10, maximum_p99_regression_percent=5,
                          maximum_live_memory_increase_percent=20, steady_state_growth_tolerance_mib=.5)
        self.rows = [dict(case='heavy', mode=mode, repeat=i, seconds=120, ups=100 if mode=='direct' else 120,
                          p99_ms=10, live_mib=100, growth_mib=0, replay=False, replay_signature='')
                     for mode in ['direct','candidate'] for i in range(1,4)]

    # A throughput win must not conceal unacceptable memory or p99 regressions.
    def test_independent_memory_and_latency_gates(self):
        for row in self.rows[3:]: row.update(live_mib=125, p99_ms=11)
        result=evaluate(self.rows,self.gates)[0]['checks']
        self.assertTrue(result['throughput'])
        self.assertFalse(result['p99']); self.assertFalse(result['live_memory'])

    # Short pilot runs cannot satisfy the sustained-run acceptance requirement.
    def test_short_collection_fails_duration(self):
        self.rows[0]['seconds']=15
        self.assertFalse(evaluate(self.rows,self.gates)[0]['checks']['duration'])

    # Fixed-tick callback phase must not reject a nominal two-minute replay by milliseconds.
    def test_fixed_tick_duration_precision(self):
        self.rows[0]['seconds']=119.996
        self.assertTrue(evaluate(self.rows,self.gates)[0]['checks']['duration'])
        self.rows[0]['seconds']=119.9
        self.assertFalse(evaluate(self.rows,self.gates)[0]['checks']['duration'])

    # Different gameplay invalidates the comparison even when performance improves.
    def test_mismatched_replay_is_rejected(self):
        for row in self.rows: row.update(replay=True,replay_signature='matching')
        self.rows[-1]['replay_signature']='diverged'
        with self.assertRaisesRegex(AssertionError,'Replay mismatch'): evaluate(self.rows,self.gates)

    # Population growth in real gameplay is not a steady-state leak test.
    def test_gameplay_growth_remains_unproven(self):
        for row in self.rows: row.update(replay=True,replay_signature='matching')
        self.assertIsNone(evaluate(self.rows,self.gates)[0]['checks']['steady_state_memory'])


if __name__ == '__main__': unittest.main()
