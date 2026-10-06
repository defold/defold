"""Ownership contracts for attaching acceptance tests to borrowed runtimes."""
import argparse
import unittest
from unittest import mock

import test_runtime


class HarnessTest(unittest.TestCase):
    # Attaching, polling and cleanup must not create or terminate a supplied device process.
    def test_borrowed_fixture_is_not_launched_or_terminated(self):
        class Borrowed(test_runtime.EngineTest):
            pass
        def response(path):
            if path == '/health':
                return 200, {'data': {'engine_instance_id': 'engine:borrowed'}}
            return 200, {'data': {'states': [{'value': True}]}}
        with mock.patch.object(test_runtime, 'ARGS', argparse.Namespace(service_url='http://127.0.0.1:8123/')), \
             mock.patch.object(Borrowed, 'request', side_effect=response), \
             mock.patch.object(test_runtime.subprocess, 'Popen') as launch:
            Borrowed.setUpClass()
            self.assertEqual('http://127.0.0.1:8123/automation-bridge/v3', Borrowed.url)
            self.assertEqual('engine:borrowed', Borrowed.runtime)
            self.assertIsNone(Borrowed.process)
            self.assertTrue(Borrowed.until(lambda: True))
            Borrowed.tearDownClass()
            launch.assert_not_called()

    # A supplied unrelated project must fail the fixture check before acceptance mutations run.
    def test_borrowed_unrelated_project_is_rejected(self):
        class Borrowed(test_runtime.EngineTest):
            pass
        with mock.patch.object(test_runtime, 'ARGS', argparse.Namespace(service_url='http://127.0.0.1:8123')), \
             mock.patch.object(Borrowed, 'request', side_effect=[
                 (200, {'data': {'engine_instance_id': 'engine:unrelated'}}),
                 (200, {'data': {'states': []}})]), \
             mock.patch.object(test_runtime.subprocess, 'Popen') as launch:
            with self.assertRaisesRegex(AssertionError, 'must run the automation acceptance fixture'):
                Borrowed.setUpClass()
            Borrowed.tearDownClass()
            launch.assert_not_called()


if __name__ == '__main__':
    unittest.main()
