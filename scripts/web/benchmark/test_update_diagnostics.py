"""Check critical-path attribution, including tail intervals and clock correlation."""
import unittest
from update_diagnostics import attribute, p99


class AttributionTest(unittest.TestCase):
    # The four sequential spans must reconstruct one interval; graphics is a subset.
    def test_interval_partition(self):
        fields=['id','browser_tick','source','dispatch_begin','dispatch_end','wake','events_end','update_end','graphics_calls','owner_queue','owner_execute','owner_return','max_owner_queue']
        run={'diagnostics':{'schema':1,'dropped':0,'fields':fields,'rows':[
            [1,1,0,10,11,12,13,20,1,1,2,1,1], [2,2,1,21,22,23,24,30,0,0,0,0,0]]},
            'result':{'measurement_clock_start':.02,'measurement_clock_end':.031}}
        trace=[dict(input_begin_us=13000,update_end_us=15000,prepare_end_us=17000,
                    publish_us=19000,render_begin_us=20000,submit_end_us=22000)]
        r=attribute(run,trace)[0]
        self.assertEqual(r['interval_ms'],11)
        self.assertEqual(r['worker_ms'],8)
        self.assertEqual(r['admission_gap_ms'],1)
        self.assertEqual(r['publication_wait_ms'],2)
        self.assertEqual(r['worker_other_ms'],6)
        self.assertEqual(r['dispatch_to_submit_ms'],12)
        self.assertEqual(r['owner_execute_ms'],2)

    # Disjoint clock epochs or missing updates must fail instead of making false attributions.
    def test_clock_mismatch_rejected(self):
        run={'diagnostics':{'schema':1,'dropped':0,'fields':['id','wake','update_end'],'rows':[[1,100,101]]},
             'result':{'measurement_clock_start':0,'measurement_clock_end':1}}
        with self.assertRaisesRegex(AssertionError,'Unmatched'):
            attribute(run,[{'input_begin_us':5}])

    # Nearest-rank p99 includes the correct tail boundary on a small sample.
    def test_percentile_boundary(self):
        self.assertEqual(p99(list(range(1,101))),99)


if __name__=='__main__':unittest.main()
