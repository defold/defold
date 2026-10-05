"""Keep incomplete or instrumented collections out of placement comparisons."""
import json
import tempfile
import unittest
from pathlib import Path
from placement_report import load


class PlacementReportTest(unittest.TestCase):
    def write_manifest(self, root, **changes):
        manifest = dict(headless=False, metrics=False, memoryProbe=True, bundle={"test":"hash"}, stackMeasure=False,
                        cases=[{'name':'bunny30k'}], modes=['direct','current'], repeats=3, runs=[])
        manifest.update(changes)
        (root/'manifest.json').write_text(json.dumps(manifest))

    # An interrupted collection must not silently report fewer repetitions.
    def test_missing_runs_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);self.write_manifest(root)
            with self.assertRaises(AssertionError):load(root)

    # Attribution traces and headless runs must not masquerade as acceptance data.
    def test_instrumented_or_headless_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            for changes in [{'metrics':True},{'headless':True},{'memoryProbe':False}]:
                self.write_manifest(root,**changes)
                with self.assertRaises(AssertionError):load(root)


if __name__=='__main__':unittest.main()
