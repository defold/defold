"""Guard against double-counting frame subsets in the memory report."""
import unittest
from stack_diagnostic_report import memory_breakdown


class BreakdownTest(unittest.TestCase):
    # Uploads/constants belong inside component total; GPU storage is kept separate.
    def test_disjoint_capacity_categories(self):
        snapshot=dict(component_frame_capacity_bytes=1000,component_upload_capacity_bytes=500,
            component_constant_capacity_bytes=200,all_sprite_frame_capacity_bytes=300,
            all_sprite_renderer_cpu_capacity_bytes=400,all_sprite_gpu_logical_bytes=5000,all_sprite_worlds=2)
        run={'case':{'name':'fixture'},'mode':'threaded','repeat':1,'stack':{'reserved':2097152},'snapshots':[snapshot]}
        r=memory_breakdown(run)
        self.assertEqual(r['component_metadata_mib']*1048576,300)
        self.assertEqual(r['worker_stack_mib'],2)
        self.assertEqual(r['gpu_sprite_logical_mib']*1048576,5000)
        self.assertEqual(r['sprite_worlds'],2)

    # Impossible subsets indicate stale/broken engine counters and must fail the report.
    def test_subsets_cannot_exceed_total(self):
        run={'case':{'name':'fixture'},'mode':'threaded','repeat':1,'snapshots':[
            {'component_frame_capacity_bytes':1000,'component_upload_capacity_bytes':2000,'all_sprite_worlds':1}]}
        with self.assertRaises(AssertionError):memory_breakdown(run)


if __name__=='__main__':unittest.main()
