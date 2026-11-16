import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parent / 'cmake/plot_ninja_log.py'
SPEC = importlib.util.spec_from_file_location('plot_ninja_log', SCRIPT)
plot = importlib.util.module_from_spec(SPEC)
with mock.patch.dict(sys.modules, {SPEC.name: plot}):
    SPEC.loader.exec_module(plot)


class NinjaLogTests(unittest.TestCase):
    def read_log(self, lines):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / '.ninja_log'
            path.write_text('# ninja log v5\n' + '\n'.join(lines) + '\n', encoding='utf-8')
            return plot._read_ninja_log(path)

    # A multi-output command must consume one timeline lane and count its duration once.
    def test_multi_output_command_is_counted_once(self):
        entries = self.read_log([
            '0\t1000\t10\tgenerated.cpp\taaaa',
            '0\t1000\t20\tgenerated.h\taaaa',
            '0\t1000\t20\tgenerated.h\taaaa',
            '0\t1000\t30\tindependent.o\tbbbb',
        ])
        self.assertEqual(2, len(entries))
        self.assertEqual(['generated.cpp', 'generated.h'], entries[0].outputs)
        assigned, lanes = plot._assign_lanes(entries)
        tasks, duration = plot._make_payload(assigned, Path('/repo'))
        self.assertEqual(2, lanes)
        self.assertEqual(1000, duration)
        self.assertEqual('generated.cpp (+1 outputs)', tasks[0]['label'])
        self.assertEqual(['generated.cpp', 'generated.h'], tasks[0]['outputs'])

    # Repeated commands in different invocations must remain visible and selectable with --latest.
    def test_identical_timings_in_separate_invocations_are_not_merged(self):
        entries = self.read_log([
            '1\t1000\t10\tresult.stamp\taaaa',
            '0\t1\t20\tCMakeFiles/cmake.verify_globs\tbbbb',
            '1\t1000\t30\tresult.stamp\taaaa',
        ])
        self.assertEqual(3, len(entries))
        self.assertEqual(entries[1:], plot._latest_invocation(entries))

    # Gamesys folder stamps belong to a batch; neither the bars nor slowest list may imply per-folder time.
    def test_gamesys_batch_label_preserves_searchable_folder_outputs(self):
        for separator in ('/', '\\'):
            with self.subTest(separator=separator):
                prefix = 'D:/a/defold/engine/gamesys/build/x86_64-win32/src/gamesys/test/.bob/'
                outputs = [(prefix + folder + '.stamp').replace('/', separator)
                           for folder in ('atlas', 'camera', 'gui')]
                entries = self.read_log([f'100\t81500\t0\t{output}\taaaa' for output in outputs])
                assigned, lanes = plot._assign_lanes(entries)
                tasks, _ = plot._make_payload(assigned, Path('/repo'))
                summary = plot._summary(entries, lanes, Path('/repo'))
                self.assertEqual(1, lanes)
                self.assertEqual('Gamesys test content batch (3 folder outputs)', tasks[0]['label'])
                self.assertEqual(tasks[0]['label'], summary['slowest'][0]['output'])
                self.assertEqual(81400, tasks[0]['duration'])
                self.assertEqual(outputs, tasks[0]['outputs'])

    # Rendering foreign CI paths must not resolve paths on the local filesystem or trigger network mounts.
    def test_foreign_output_paths_are_not_resolved(self):
        with mock.patch.object(Path, 'resolve', side_effect=AssertionError('Unexpected filesystem access')):
            self.assertEqual('/home/runner/work/engine.o',
                             plot._short_output('/home/runner/work/engine.o', Path('/repo')))
            self.assertEqual('engine/output.o', plot._short_output('/repo/engine/output.o', Path('/repo')))


if __name__ == '__main__':
    unittest.main()
