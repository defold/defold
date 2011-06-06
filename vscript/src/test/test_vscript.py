import unittest
from vscript_ddf_pb2 import VisualScript
from google.protobuf import text_format

from interpreter import Interpreter
import sys, os

class TestVScript(unittest.TestCase):

    def test01(self):
        script_source = open('src/test/data/test01.vscript', 'r').read()

        interpreter = Interpreter(script_source)
        interpreter.run()

if __name__ == '__main__':
    unittest.main()
