import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(__file__))

import solution_xcode


class SolutionXcodeTest(unittest.TestCase):
    def test_dmengine_application_scheme_is_first(self):
        targets = [
            {
                'id': 'A' * 24,
                'name': 'run_tests',
                'buildable_name': 'run_tests',
                'target_type': 'PBXAggregateTarget',
                'product_type': '',
            },
            {
                'id': 'B' * 24,
                'name': 'test_alpha',
                'buildable_name': 'test_alpha',
                'target_type': 'PBXNativeTarget',
                'product_type': 'com.apple.product-type.tool',
            },
            {
                'id': 'C' * 24,
                'name': 'dmengine',
                'buildable_name': 'dmengine',
                'target_type': 'PBXNativeTarget',
                'product_type': 'com.apple.product-type.application',
            },
        ]

        scheme_targets = solution_xcode._get_scheme_targets(targets)
        scheme_names = [target.get('scheme_name', target['name']) for target in scheme_targets]

        self.assertEqual(['dmengine', 'Run Tests', 'test_alpha'], scheme_names)

    def test_dmengine_application_scheme_is_runnable(self):
        target = {
            'id': 'C' * 24,
            'name': 'dmengine',
            'buildable_name': 'dmengine',
            'target_type': 'PBXNativeTarget',
            'product_type': 'com.apple.product-type.application',
        }

        scheme = solution_xcode._make_scheme(
            '/tmp/Defold.xcodeproj',
            target,
            'RelWithDebInfo',
            '/tmp/defold',
            '/tmp/defold',
            '/tmp/dynamo_home')

        self.assertIn('<LaunchAction', scheme)
        self.assertIn('<BuildableProductRunnable', scheme)
        self.assertIn('BlueprintName = "dmengine"', scheme)

    def test_ios_app_test_scheme_launches_product_directly(self):
        target = {
            'id': 'E' * 24,
            'name': 'test_align',
            'buildable_name': 'test_align.app',
            'target_type': 'PBXNativeTarget',
            'product_type': 'com.apple.product-type.application',
            'working_directory': '/tmp/defold/engine/dlib',
            'ios_runner_platform': 'device',
            'configfile': 'unittest.cfg',
            'stage_pairs': [('/tmp/defold/engine/dlib/src/test/data', 'src/test/data')],
        }
        solution_xcode._configure_ios_app_test_launch(target)

        scheme = solution_xcode._make_scheme(
            '/tmp/Defold.xcodeproj',
            target,
            'RelWithDebInfo',
            '/tmp/defold',
            '/tmp/defold',
            '/tmp/dynamo_home')

        self.assertIn('<BuildableProductRunnable', scheme)
        self.assertNotIn('<PathRunnable', scheme)
        self.assertNotIn('build_ios.py', scheme)
        self.assertIn('BuildableName = "test_align.app"', scheme)
        self.assertIn('argument = "./unittest.cfg"', scheme)
        self.assertIn('key = "DEFOLD_TEST_WORKDIR"', scheme)
        self.assertIn('value = "@executable_path/defold-tests/dlib"', scheme)
        self.assertIn('key = "DEFOLD_TEST_READY_PATHS"', scheme)
        self.assertIn('value = "src;src/test;src/test/data;unittest.cfg"', scheme)
        self.assertIn('useCustomWorkingDirectory = "NO"', scheme)

    def test_read_test_scheme_metadata_with_ios_runner_fields(self):
        with tempfile.TemporaryDirectory() as tmp:
            xcode_project_path = os.path.join(tmp, 'Defold.xcodeproj')
            os.makedirs(xcode_project_path)
            with open(os.path.join(tmp, 'defold_xcode_test_schemes.tsv'), 'w', encoding='utf-8') as f:
                f.write('test_align\t/tmp/defold/engine/dlib\tarm64-ios\tdevice\tunittest.cfg\tbuild/default\tbuild/default\n')

            metadata = solution_xcode._read_test_scheme_metadata(xcode_project_path)

        self.assertEqual('/tmp/defold/engine/dlib', metadata['test_align']['working_directory'])
        self.assertEqual('arm64-ios', metadata['test_align']['target_platform'])
        self.assertEqual('device', metadata['test_align']['ios_runner_platform'])
        self.assertEqual('unittest.cfg', metadata['test_align']['configfile'])
        self.assertEqual([('build/default', 'build/default')], metadata['test_align']['stage_pairs'])


if __name__ == '__main__':
    unittest.main()
