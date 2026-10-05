"""Keep retained-memory changes distinct from reclaimed diagnostic garbage."""
import unittest
from owned_memory_report import diagnostic_row


class OwnedMemoryReportTest(unittest.TestCase):
    # Reclaiming garbage at the end must not be confused with retained growth.
    def test_retained_growth_and_reclaimed_bytes_are_separate(self):
        run = {'memoryDiagnostic': True, 'mode': 'owned', 'result': {'elapsed_seconds':120},
               'gc': [{'phase':'start','allocated_after':1000,'lua_after':100},
                      {'phase':'end','allocated_before':4000,'allocated_after':1100,
                       'lua_before':2000,'lua_after':110}]}
        row = diagnostic_row(run, True)
        self.assertEqual(row['post_gc_growth_bytes'],100)
        self.assertEqual(row['end_reclaimed_bytes'],2900)
        self.assertEqual(row['lua_post_gc_growth_bytes'],10)
        self.assertEqual(row['lua_reclaimed_bytes'],1890)

    # Missing or reversed GC boundaries must never produce an apparent stable baseline.
    def test_reversed_boundaries_are_rejected(self):
        run = {'memoryDiagnostic':True, 'gc':[{'phase':'end'},{'phase':'start'}]}
        with self.assertRaises(AssertionError): diagnostic_row(run,False)


if __name__ == '__main__': unittest.main()
