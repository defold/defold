"""Target decisions must preserve failures and missing percentile evidence."""
import unittest
from pacing_report import gates, capacity_gates, validate_pairing, CAPACITIES


class GateTests(unittest.TestCase):
    # Pairing must reject different binaries and duplicate measurements before publication.
    def test_pairing_rejects_mismatched_or_duplicate_evidence(self):
        manifest=dict(statusReports=False,memorySamples=False,modes=['poc_direct','threaded_ready'],
                      bundle={'poc_direct':{'wasm':'same'},'threaded_ready':{'wasm':'same'}},
                      cases=[{'name':'bunny30k'}],repeats=3)
        validate_pairing([manifest])
        with self.assertRaisesRegex(AssertionError,'Repeated evidence'):validate_pairing([manifest,manifest])
        manifest['bundle']['threaded_ready']['wasm']='different'
        with self.assertRaisesRegex(AssertionError,'identical engine'):validate_pairing([manifest])

    # A single successful replay proves compatibility, not a repeated performance target.
    def test_single_replay_is_not_a_performance_gate(self):
        rows=[dict(case='space_game',mode=mode,updates_per_second=50,p99_ms=20)
              for mode in ['poc_direct','threaded_ready','threaded_budget']]
        self.assertEqual(gates(rows),[])

    # Compare every retained-capacity category; aggregate memory noise cannot hide added slots.
    def test_capacity_increase_is_visible(self):
        rows=[dict(case='bunny30k',mode=mode,**{field:100 for field in CAPACITIES})
              for mode in ['threaded_ready','threaded_budget']]
        self.assertTrue(capacity_gates(rows)[0]['no_extra_capacity_pass'])
        rows[-1][CAPACITIES[0]]+=1
        self.assertFalse(capacity_gates(rows)[0]['no_extra_capacity_pass'])

    # Independent throughput and p99 gates must not hide a tail regression behind a gain.
    def test_tail_regression_is_not_a_throughput_pass(self):
        rows=[dict(case='bunny30k',mode=mode,updates_per_second=ups,p99_ms=p99)
              for mode,ups,p99 in [('poc_direct',30,50),('threaded_ready',40,60),('threaded_budget',39,53)]]
        gate=gates(rows)[0]
        self.assertTrue(gate['throughput_90_percent_pass'])
        self.assertFalse(gate['sprite_p99_105_percent_pass'])
        rows[-1]['p99_ms']=52.5
        self.assertTrue(gates(rows)[0]['sprite_p99_105_percent_pass'])

    # Censored tails remain unavailable; they cannot be turned into a finite passing value.
    def test_missing_p99_is_not_fabricated(self):
        rows=[dict(case='geometry1000',mode=mode,updates_per_second=1,p99_ms=None)
              for mode in ['poc_direct','threaded_ready','threaded_budget']]
        gate=gates(rows)[0]
        self.assertIsNone(gate['p99_ratio_to_direct'])
        self.assertIsNone(gate['sprite_p99_105_percent_pass'])


if __name__=='__main__': unittest.main()
