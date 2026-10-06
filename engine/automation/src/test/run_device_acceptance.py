"""Run fixture acceptance through a supplied device/simulator service URL and retain local evidence."""
import argparse
import json
from pathlib import Path
import platform
import sys
import unittest

import test_capture
import test_runtime


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--service-url', required=True, help='Concrete forwarded or directly reachable engine service URL')
    parser.add_argument('--platform', required=True, choices=('ios', 'android', 'macos', 'linux', 'windows'))
    parser.add_argument('--adapter', required=True, choices=('metal', 'opengl', 'opengles', 'vulkan'))
    parser.add_argument('--device-kind', required=True, choices=('physical', 'simulator', 'desktop'),
                        help='Provenance supplied by the launcher; never inferred from cross-compilation')
    parser.add_argument('--device-name', required=True, help='Device or simulator name from its launcher')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    test_runtime.ARGS = argparse.Namespace(service_url=args.service_url, adapter=args.adapter,
                                           engine=None, fixture=str(args.output), extension=False)
    report = dict(host=platform.platform(), expected_platform=args.platform, expected_adapter=args.adapter,
                  declared_device_kind=args.device_kind, declared_device_name=args.device_name, status='failed')
    report_path = args.output / 'acceptance.json'
    try:
        test_runtime.EngineTest.setUpClass()
        health = test_runtime.EngineTest.request('/health')[1]['data']
        report['health'] = health
        if health['platform'] != args.platform or health['backend']['adapter'] != args.adapter:
            raise AssertionError('service platform/adapter does not match the requested acceptance target')
        # Packaged resources cannot be hot-reloaded. DAP needs its own separately forwarded port.
        excluded = {'test_hot_reload_identity_and_owner_cleanup', 'test_health_and_headless_capabilities',
                    'test_debugger_pause_and_resume', 'test_z_reboot_invalidates_runtime_and_snapshots'}
        names = [name for name in unittest.defaultTestLoader.getTestCaseNames(test_runtime.RuntimeTest)
                 if name not in excluded]
        report['runtime_tests'] = names
        runner = unittest.TextTestRunner(verbosity=2)
        results = []
        try:
            results.append(runner.run(unittest.TestSuite(test_runtime.RuntimeTest(name) for name in names)))
        finally:
            test_runtime.EngineTest.request('/input/flush', 'POST',
                {'client_id': 'runtime-test', 'session_id': 'fixture', 'release': True})
        results.append(runner.run(unittest.defaultTestLoader.loadTestsFromTestCase(test_capture.CaptureTest)))
        results.append(runner.run(unittest.TestSuite([test_runtime.RuntimeTest('test_z_reboot_invalidates_runtime_and_snapshots')])))
        report['tests_run'] = sum(result.testsRun for result in results)
        report['failures'] = [dict(test=str(test), traceback=trace) for result in results
                              for test, trace in result.failures + result.errors]
        report['skipped'] = [dict(test=str(test), reason=reason) for result in results for test, reason in result.skipped]
        report['status'] = 'passed' if all(result.wasSuccessful() and not result.skipped for result in results) else 'failed'
    except Exception as error:
        report['error'] = str(error)
        print(str(error), file=sys.stderr)
    finally:
        # The runner owns no device process and must never terminate the supplied runtime.
        test_runtime.EngineTest.tearDownClass()
        report_path.write_text(json.dumps(report, indent=2) + '\n')
    print(f"Acceptance {report['status']}: {report_path}")
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    sys.exit(main())
