"""Regression checks for memory-domain and run-validity reporting."""
import copy
import unittest
from memory_report import analyze_memory, MIB


class MemoryReportTest(unittest.TestCase):
    def fixture(self):
        return dict(valid=True, exit=0, case={'name':'bunny30k'}, mode='before_threaded', repeat=1,
            result={'valid':True,'engine':{'is_debug':False},'update_intervals_per_second':100},
            timing={'samples':100,'overflow_samples':0,'p99_ms_upper_bound':12.5},
            memory=[{'allocator':{'allocated':30*MIB,'free':5*MIB,'arena':35*MIB},'heap':50*MIB,'rss':80*MIB,'hidden':False,'focused':True},
                    {'allocator':{'allocated':20*MIB,'free':15*MIB,'arena':35*MIB},'heap':50*MIB,'rss':80*MIB,'hidden':False,'focused':True}],
            snapshots=[{'slots_payload_used_bytes':4*MIB,'frame_capacity_bytes':5*MIB,'frame_growth_peak_bytes':7*MIB,
                        'renderer_cpu_capacity_bytes':2*MIB,'renderer_gpu_logical_bytes':3*MIB,'record_bytes':96}],
            stack={'reserved':2*MIB})

    # Avoid treating capacity as live usage or adding overlapping stack/snapshot totals.
    def test_live_peak_is_separate_from_capacity_and_subtotals(self):
        row = analyze_memory(self.fixture())
        self.assertEqual(row['allocator_peak_mib'],30)
        self.assertEqual(row['wasm_peak_mib'],50)
        self.assertEqual(row['snapshot_capacity_mib'],5)
        self.assertEqual(row['worker_stack_mib'],2)
        self.assertEqual(row['allocator_free_last_mib'],15)

    # Reject invalid timings and unfocused runs instead of silently graphing them.
    def test_invalid_measurements_are_rejected(self):
        for kind in ['focus','overflow','debug']:
            run = copy.deepcopy(self.fixture())
            if kind == 'focus': run['memory'][0]['focused'] = False
            if kind == 'overflow': run['timing']['overflow_samples'] = 1
            if kind == 'debug': run['result']['engine']['is_debug'] = True
            with self.assertRaises(AssertionError): analyze_memory(run)


if __name__ == '__main__':
    unittest.main()
