import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(__file__))

import sdk


class SDKTest(unittest.TestCase):
    def test_compile_file_clang_passes_ios_sysroot_path_as_one_argument(self):
        commands = []
        shell_commands = []
        original_command = sdk.run.command
        original_shell_command = sdk.run.shell_command
        try:
            def fake_command(args, **kwargs):
                commands.append(args)
                return ''

            def fake_shell_command(args, **kwargs):
                shell_commands.append(args)
                if args == 'which clang++':
                    return '/usr/bin/clang++'
                return ''

            sdk.run.command = fake_command
            sdk.run.shell_command = fake_shell_command

            sysroot = '/Users/test/Downloads/Xcode 26.5.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS26.5.sdk'
            sdk._compile_file_clang(
                'arm64-ios',
                {'arm64-ios': {'path': sysroot}},
                '/tmp/hello.cpp',
                '/tmp/a.out',
                verbose=False)
        finally:
            sdk.run.command = original_command
            sdk.run.shell_command = original_shell_command

        self.assertEqual(['which clang++'], shell_commands)
        self.assertEqual(1, len(commands))
        self.assertEqual([
            'clang++',
            '-isysroot',
            sysroot,
            '-arch',
            'arm64',
            '/tmp/hello.cpp',
            '-o',
            '/tmp/a.out',
        ], commands[0])


if __name__ == '__main__':
    unittest.main()
