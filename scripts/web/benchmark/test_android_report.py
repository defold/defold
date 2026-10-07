"""Regression checks for the anonymous mobile evidence exporter."""
import json
import unittest
from android_report import summarize


def fixture():
    temperature={'status':0,'idleCpu':{'idleFraction':0.99},'temperatures':[{'type':3,'celsius':34}]}
    sample={'phase':'measure','focused':True,'hidden':False,'canvas':[1280,720],
            'dpr':2.75,'allocator':{'allocated':12*1048576},'heap':32*1048576}
    return {'valid':True,'exit':0,'errors':[],'case':{'name':'space_game','scene':'replay'},
            'mode':'threaded','repeat':1,'focusEmulation':False,'timing':{'overflow_samples':0,'p99_ms_upper_bound':20,'samples':100,'histogram_limit_ms':1000},
            'memory':[sample,sample], 'thermalBefore':temperature,'thermal':[temperature],'thermalAfter':temperature,
            'power':{'powered':True,'temperatureC':30},'powerAfter':{'powered':True,'temperatureC':32},
            'replaySignature':'PRIVATE_SIGNATURE', 'log':['PRIVATE_LOG'],
            'result':{'valid':True,'engine':{'is_debug':False},'cleanup_verified':True,
                      'elapsed_seconds':120,'update_intervals_per_second':50,
                      'replay':{'source':'PRIVATE_SOURCE','checkpoints':['PRIVATE_STATE']},
                      'memory':{'samples':[{'allocated_bytes':15*1048576}],
                                'measurement_start':{'allocated_bytes':11*1048576},
                                'measurement_end':{'allocated_bytes':13*1048576},
                                'after_delete':{'allocated_bytes':5*1048576}}}}


class ExportTests(unittest.TestCase):
    # Raw state, logs and identifiers must never enter the anonymous metrics export.
    def test_export_selects_only_anonymous_metrics(self):
        row=summarize(fixture())
        self.assertNotIn('PRIVATE_',json.dumps(row))
        self.assertEqual(row['peak_live_mib'],15)
        self.assertEqual(row['cleanup_live_mib'],5)
        self.assertTrue(row['replay_validated'])
        self.assertNotIn('rss',row)

    # Incomplete replay evidence and unreviewed case labels must fail before publication.
    def test_rejects_incomplete_or_unapproved_evidence(self):
        for field,value in [('cleanup_verified',False),('valid',False)]:
            run=fixture();run['result'][field]=value
            with self.assertRaises(AssertionError): summarize(run)
        run=fixture();run['case']['name']='unreviewed-project-name'
        with self.assertRaises(AssertionError): summarize(run)

    # Thermal changes during valid work must remain visible, not be filtered for speed.
    def test_preserves_later_thermal_status(self):
        run=fixture();run['thermalAfter']={'status':2,'temperatures':[{'type':3,'celsius':45}]}
        row=summarize(run)
        self.assertEqual(row['max_thermal_status'],2)
        self.assertEqual(row['peak_skin_c'],45)

    # Slow geometry must publish an unknown p99 with its bound, never clamp it to 1 s.
    def test_geometry_overflow_is_explicit_and_censored(self):
        run=fixture();run['case']={'name':'geometry1000','scene':'geometry'}
        run['timing']={'samples':45,'overflow_samples':4,'histogram_limit_ms':1000,'max_ms':1122}
        with self.assertRaises(AssertionError): summarize(run)
        run['timingOverflowAllowed']=True
        row=summarize(run)
        self.assertIsNone(row['p99_ms'])
        self.assertEqual(row['p99_lower_bound_ms'],1000)
        self.assertEqual(row['timing_overflow_samples'],4)
        run['case']={'name':'space_game','scene':'replay'}
        with self.assertRaises(AssertionError): summarize(run)


if __name__=='__main__': unittest.main()
