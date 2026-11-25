import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import coverage_report  # noqa: E402


def summary_entry(filename, lines, functions=(1, 1), branches=(0, 0)):
    return {'filename': filename, 'summary': {
        'lines': {'covered': lines[0], 'count': lines[1]},
        'functions': {'covered': functions[0], 'count': functions[1]},
        'branches': {'covered': branches[0], 'count': branches[1]}}}


def write_summary(path, entries):
    path.write_text(json.dumps({'data': [{'files': entries}]}))


class ChangedLineTests(unittest.TestCase):
    # Added and modified lines come from the new side of each hunk; deleted files and pure deletions add nothing.
    def test_parse_changed_lines(self):
        diff = '\n'.join([
            'diff --git a/engine/dlib/src/dlib/a.cpp b/engine/dlib/src/dlib/a.cpp',
            '--- a/engine/dlib/src/dlib/a.cpp',
            '+++ b/engine/dlib/src/dlib/a.cpp',
            '@@ -10,0 +11,3 @@ void F()',
            '+x', '+y', '+z',
            '@@ -20 +23 @@',
            '-old', '+new',
            '@@ -30,2 +32,0 @@',
            '-gone', '-gone',
            'diff --git a/engine/dlib/src/dlib/b.cpp b/engine/dlib/src/dlib/b.cpp',
            '--- a/engine/dlib/src/dlib/b.cpp',
            '+++ /dev/null',
            '@@ -1,2 +0,0 @@',
            '-a', '-b',
        ])
        self.assertEqual({'engine/dlib/src/dlib/a.cpp': {11, 12, 13, 23}},
                         coverage_report.parse_changed_lines(diff))

    # Only instrumented lines count; comments and lines of files outside the report are ignored.
    def test_changed_line_coverage(self):
        lcov = {'engine/dlib/src/dlib/a.cpp': {11: 4, 12: 0, 23: 0}}
        changed = {'engine/dlib/src/dlib/a.cpp': {11, 12, 13, 23}, 'engine/dlib/src/test/t.cpp': {1}}
        result = coverage_report.changed_line_coverage(lcov, changed)
        self.assertEqual(1, result['covered'])
        self.assertEqual(3, result['count'])
        self.assertEqual({'covered': [11], 'uncovered': [12, 23]}, result['files']['engine/dlib/src/dlib/a.cpp'])
        self.assertEqual('12, 23-25', coverage_report.line_ranges([12, 23, 24, 25]))
        self.assertEqual('1, 3, … (2 more)', coverage_report.line_ranges([1, 3, 5, 7], 2))

    # Coverage of a line is the highest count of any record, so duplicate SF records merge.
    def test_read_lcov(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            path = root / 'lcov.info'
            source = root / 'engine/dlib/src/dlib/a.cpp'
            path.write_text('SF:%s\nDA:1,0\nDA:2,3,abc\nend_of_record\nSF:%s\nDA:1,5\nend_of_record\n' % (source, source))
            self.assertEqual({'engine/dlib/src/dlib/a.cpp': {1: 5, 2: 3}}, coverage_report.read_lcov(path, root))


class SummaryTests(unittest.TestCase):
    # A baseline from another checkout path compares per library and lists the largest file drops first.
    def test_markdown_with_baseline_and_changed_lines(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            write_summary(root / 'now.json', [
                summary_entry(str(root / 'engine/dlib/src/dlib/a.cpp'), (5, 10)),
                summary_entry(str(root / 'engine/dlib/src/dlib/new.cpp'), (2, 2)),
                summary_entry(str(root / 'engine/gui/src/gui.cpp'), (9, 10))])
            write_summary(root / 'before.json', [
                summary_entry('/ci/defold/engine/dlib/src/dlib/a.cpp', (8, 10)),
                summary_entry('/ci/defold/engine/dlib/src/dlib/old.cpp', (1, 1)),
                summary_entry('/ci/defold/engine/gui/src/gui.cpp', (9, 10))])
            files = coverage_report.read_summary(root / 'now.json', root)
            baseline = coverage_report.read_summary(root / 'before.json', Path('/ci/defold'))
            changed = {'covered': 1, 'count': 3, 'files': {
                'engine/dlib/src/dlib/a.cpp': {'covered': [11], 'uncovered': [12, 23]}}}
            text = coverage_report.markdown_summary(files, baseline, 'abc123', changed, 'abc123')
        self.assertIn('| dlib | 58.3% -23.5 (7/12) |', text)
        self.assertIn('| gui | 90.0% +0.0 (9/10) |', text)
        self.assertIn('| **Total** | **72.7%** -13.0 (16/22) |', text)
        drops = text.split('### Largest line coverage changes')[1]
        self.assertLess(drops.index('old.cpp'), drops.index('a.cpp'))
        self.assertLess(drops.index('a.cpp'), drops.index('new.cpp'))
        self.assertNotIn('gui.cpp', drops)
        self.assertIn('33.3% of changed executable lines are covered (1/3).', text)
        self.assertIn('| `engine/dlib/src/dlib/a.cpp` | 1/3 | 12, 23 |', text)

    # Without a baseline or diff base, the summary is the plain per-library table.
    def test_markdown_without_baseline(self):
        files = {'engine/dlib/src/dlib/a.cpp': {'lines': (1, 2), 'functions': (1, 1), 'branches': (0, 0)}}
        text = coverage_report.markdown_summary(files)
        self.assertIn('| dlib | 50.0% (1/2) | 100.0% (1/1) | - (0/0) |', text)
        self.assertNotIn('###', text)


class GitDiffTests(unittest.TestCase):
    # The diff against a commit includes uncommitted edits, matching the sources the report was built from.
    def test_working_tree_diff(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            subprocess.run(['git', 'init', '-q', str(root)], check=True)
            source = root / 'engine/dlib/src/a.cpp'
            source.parent.mkdir(parents=True)
            source.write_text('a\nb\n')
            subprocess.run(['git', '-C', str(root), 'add', '.'], check=True)
            subprocess.run(['git', '-C', str(root), '-c', 'user.name=t', '-c', 'user.email=t@t', 'commit', '-qm', 'base'],
                           check=True)
            source.write_text('a\nchanged\nc\n')
            diff = coverage_report.git(root, 'diff', '--unified=0', '--no-color', '--no-ext-diff',
                                       '--src-prefix=a/', '--dst-prefix=b/', 'HEAD', '--')
            self.assertEqual({'engine/dlib/src/a.cpp': {2, 3}}, coverage_report.parse_changed_lines(diff))


if __name__ == '__main__':
    unittest.main()
