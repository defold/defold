"""Protect source projects and reject incompatible replay adapters before copying."""
import json
import tempfile
import unittest
from pathlib import Path
from prepare_replay import prepare


class PrepareReplayTest(unittest.TestCase):
    # A disposable-copy operation must never recurse into or overwrite its source.
    def test_reject_source_destination(self):
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)
            for destination in [source,source/'nested-copy']:
                with self.assertRaisesRegex(ValueError,'outside the source'):
                    prepare(source,destination,source/'unused.json')

    # Project-specific scenario replacement must fail without leaving a partial copy.
    def test_wrong_adapter_leaves_no_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);source=root/'source';source.mkdir()
            (source/'game.project').write_text('[project]\n')
            manifest=source/'manifest.json';manifest.write_text(json.dumps({'expected':{'id':'another-game'}}))
            with self.assertRaisesRegex(ValueError,'reviewed offline Underwatermelon'):
                prepare(source,root/'output',manifest,True)
            self.assertFalse((root/'output').exists())


if __name__ == '__main__': unittest.main()
