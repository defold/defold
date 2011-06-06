import unittest
from vscript_ddf_pb2 import VisualScript
from google.protobuf import text_format

import sys, os

class TestVScript(unittest.TestCase):
    
    def test01(self):
        script_source = open('src/test/data/test01.vscript', 'r').read()
        
        script = VisualScript()
        text_format.Merge(script_source, script)
        print script
        self.assertEqual(1, 1) 

if __name__ == '__main__':
    unittest.main()
