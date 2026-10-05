"""Ensure temporary screen-saver changes are restored when collection fails."""
import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch
import awake_run


class AwakeRunTest(unittest.TestCase):
    # A crashed collector must not leave the user's screen saver disabled.
    def test_restore_after_collector_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            settings=Path(directory)/'settings.json'
            child=Mock();child.wait.side_effect=RuntimeError('collector failed');child.poll.return_value=1
            read=subprocess.CompletedProcess([],0,'1200\n','')
            with patch('sys.argv',['awake_run','--settings',str(settings),'--','node','run.cjs']), \
                 patch('awake_run.signal.signal'), patch('awake_run.subprocess.run',return_value=read) as run, \
                 patch('awake_run.subprocess.Popen',return_value=child):
                with self.assertRaisesRegex(RuntimeError,'collector failed'): awake_run.main()
            self.assertEqual(json.loads(settings.read_text()),{'existed':True,'idleTime':1200})
            self.assertEqual(run.call_args.args[0],['defaults','-currentHost','write','com.apple.screensaver','idleTime','-int','1200'])


if __name__ == '__main__': unittest.main()
