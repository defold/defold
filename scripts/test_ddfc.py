import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import unittest


class DdfCompilerTests(unittest.TestCase):
    # An incompatible site-installed protobuf must not break SDK DDF generation.
    def test_cpp_generation_ignores_site_packages(self):
        root = Path(__file__).resolve().parent.parent
        sdk = Path(os.environ.get('DYNAMO_HOME', root / 'tmp/dynamo_home'))
        host = {'Darwin': 'macos', 'Linux': 'linux', 'Windows': 'win32'}[platform.system()]
        arch = 'arm64' if platform.machine().lower() in ('arm64', 'aarch64') else 'x86_64'
        protoc = sdk / 'ext/bin' / f'{arch}-{host}' / ('protoc.exe' if host == 'win32' else 'protoc')
        plugin = root / 'engine/ddf/src' / ('ddfc_cxx.bat' if host == 'win32' else 'ddfc_cxx')
        if not protoc.is_file() or not (sdk / 'lib/python/plugin_pb2.py').is_file():
            self.skipTest('Requires an installed host SDK with protoc and DDF Python modules')

        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            # Python loads sitecustomize from PYTHONPATH unless -S is used.
            # Reproduce a native protobuf module incompatible with the SDK runtime.
            (output / 'sitecustomize.py').write_text('''import sys
class IncompatibleProtobuf:
    def find_spec(self, fullname, path=None, target=None):
        if fullname == 'google._upb._message':
            raise AttributeError('incompatible site-installed protobuf')
sys.meta_path.insert(0, IncompatibleProtobuf())
''')
            env = os.environ.copy()
            env['PYTHONPATH'] = os.pathsep.join(map(str, [
                output, sdk / 'lib/python', sdk / 'ext/lib/python',
            ]))
            env['PATH'] = str(Path(sys.executable).parent) + os.pathsep + env['PATH']
            control = subprocess.run(
                [sys.executable, '-c', 'from google.protobuf import descriptor'],
                env=env, capture_output=True, text=True)
            self.assertNotEqual(0, control.returncode)
            self.assertIn('incompatible site-installed protobuf', control.stderr)

            result = subprocess.run([
                str(protoc), f'--plugin=protoc-gen-ddf={plugin}',
                f'--ddf_out={output}',
                '-I', str(root / 'engine/font/proto/font'),
                '-I', str(root / 'engine/ddf/src'),
                '-I', str(sdk / 'ext/include'),
                str(root / 'engine/font/proto/font/glyphbank_ddf.proto'),
            ], env=env, capture_output=True, text=True)
            self.assertEqual(0, result.returncode, result.stderr)
            self.assertIn('struct GlyphBank', (output / 'glyphbank_ddf.h').read_text())
            self.assertIn('dmFontDDF', (output / 'glyphbank_ddf.cpp').read_text())


if __name__ == '__main__':
    unittest.main()
