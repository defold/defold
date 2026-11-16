import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / 'engine/gamesys/src/gamesys/test/build_content_batch.py'
SPEC = importlib.util.spec_from_file_location('gamesys_content_batch', SCRIPT)
batch = importlib.util.module_from_spec(SPEC)
with mock.patch.object(sys, 'path', [str(SCRIPT.parent), *sys.path]):
    SPEC.loader.exec_module(batch)


class GamesysContentTimingTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='gamesys timings ')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.stage = self.root / 'stage'
        self.stage.mkdir()
        (self.stage / 'common_build.inputs').write_text('', encoding='utf-8')
        self.output = self.root / 'output'
        self.jobs = []
        for folder in ('atlas', 'animationset', 'gui'):
            source = self.stage / folder
            source.mkdir()
            (source / 'build.inputs').write_text('', encoding='utf-8')
            stamp = self.root / 'stamps' / (folder + '.stamp')
            stamp.parent.mkdir(exist_ok=True)
            stamp.write_text('old stamp\n', encoding='utf-8')
            self.jobs.append({'folder': folder, 'stamp': str(stamp), 'dependencies': []})
        self.clock = 0.0
        self.worker = mock.MagicMock()
        self.worker.__enter__.return_value = self.worker
        self.worker.invoke.side_effect = self.invoke
        self.args = [str(SCRIPT), '--manifest', str(self.root / 'lane.json'),
                     '--root', str(self.stage), '--stage', str(self.stage),
                     '--output-root', str(self.output), '--source-stamp', str(self.root / 'source.stamp'),
                     '--bob-worker', str(self.root / 'classes'), '--classpath', 'unused.jar',
                     '--java', 'java', '--platform', 'x86_64-win32', '--max-cpu-threads', '2']
        self.original_replace_tree = batch.replace_tree

    def invoke(self, command):
        self.assertFalse((self.stage / '.internal').exists())
        self.assertFalse((self.stage / 'build').exists())
        self.clock += 2.0
        (self.stage / '.internal').mkdir()
        destination = self.stage / command[command.index('--output') + 1]
        destination.mkdir(parents=True)
        (destination / 'content.bin').write_bytes(b'content')

    def publish(self, source, destination):
        self.clock += 3.0
        self.original_replace_tree(source, destination)

    def run_batch(self, jobs, output):
        with mock.patch.object(sys, 'argv', self.args), \
                mock.patch.object(batch, 'dirty_jobs', return_value=jobs), \
                mock.patch.object(batch, 'BobWorker', return_value=self.worker) as constructor, \
                mock.patch.object(batch, 'replace_tree', side_effect=self.publish), \
                mock.patch.object(batch.time, 'monotonic', side_effect=lambda: self.clock), \
                contextlib.redirect_stdout(output):
            batch.main()
            return constructor

    # Every rebuilt folder reports its own elapsed phases while preserving output publication and stamps.
    def test_successful_folders_have_individual_timings(self):
        output = io.StringIO()
        constructor = self.run_batch(self.jobs, output)
        constructor.assert_called_once()
        self.assertEqual(3, self.worker.invoke.call_count)
        for job in self.jobs:
            folder = job['folder']
            self.assertIn(f'Gamesys test content timing [x86_64-win32] {folder}: completed '
                          'total=5.000s prepare=0.000s bob=2.000s publish=3.000s', output.getvalue())
            self.assertEqual(b'content', (self.output / folder / 'content.bin').read_bytes())
            self.assertEqual('ok\n', Path(job['stamp']).read_text(encoding='utf-8'))

    # A failed Bob invocation must report elapsed time, retain completed stamps, and stop subsequent folders.
    def test_failure_reports_partial_timing_without_marking_success(self):
        failure = subprocess.CalledProcessError(7, ['java'])
        first_invoke = self.invoke

        def invoke(command):
            if command[command.index('--output') + 1] == 'build/animationset':
                self.clock += 4.0
                raise failure
            first_invoke(command)

        self.worker.invoke.side_effect = invoke
        output = io.StringIO()
        with self.assertRaises(subprocess.CalledProcessError) as raised:
            self.run_batch(self.jobs, output)
        self.assertIs(failure, raised.exception)
        self.assertEqual(2, self.worker.invoke.call_count)
        self.assertIn('animationset: failed total=4.000s prepare=0.000s bob=4.000s', output.getvalue())
        self.assertNotIn('gui:', output.getvalue())
        self.assertEqual('ok\n', Path(self.jobs[0]['stamp']).read_text(encoding='utf-8'))
        self.assertFalse(Path(self.jobs[1]['stamp']).exists())
        self.assertEqual('old stamp\n', Path(self.jobs[2]['stamp']).read_text(encoding='utf-8'))

    # Up-to-date folders must not start a JVM or emit misleading build timing rows.
    def test_clean_batch_has_no_folder_timings(self):
        output = io.StringIO()
        constructor = self.run_batch([], output)
        constructor.assert_not_called()
        self.assertEqual('', output.getvalue())


if __name__ == '__main__':
    unittest.main()
