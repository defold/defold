import unittest
from vscript_ddf_pb2 import VisualScript
from google.protobuf import text_format

from interpreter import Interpreter
import sys, os

class TestVScript(unittest.TestCase):

    def parse(self, name):
        script_source = open('src/test/data/%s.vscript' % name, 'r').read()
        return Interpreter(script_source)

    def test01(self):
        interpreter = self.parse('test01')
        interpreter.run()

    def test_set_variable(self):
        interpreter = self.parse('test_set_variable')
        interpreter.run()
        self.assertEquals(interpreter.get_global('x'), 123)

    def test_get_variable(self):
        interpreter = self.parse('test_get_variable')
        interpreter.set_global('a_constant', 10000)
        interpreter.run()
        self.assertEquals(interpreter.get_global('x'), 10000)

    def test_constants(self):
        interpreter = self.parse('test_constants')
        interpreter.run()
        self.assertEquals(interpreter.get_global('number'), 123)
        self.assertEquals(interpreter.get_global('string'), 'hello')

    def test_if1(self):
        interpreter = self.parse('test_if1')
        interpreter.run()
        self.assertEquals(interpreter.get_global('x'), 1010)

    def test_if_else1(self):
        interpreter = self.parse('test_if_else1')
        interpreter.run()
        self.assertEquals(interpreter.get_global('x'), 2)

    def test_invoke_statement(self):
        interpreter = self.parse('test_invoke_statement')
        interpreter.run()
        self.assertEquals(interpreter.get_global('x'), 123)

    def test_call_expression(self):
        interpreter = self.parse('test_call_expression')
        interpreter.run()
        self.assertEquals(interpreter.get_global('x'), 16)

if __name__ == '__main__':
    unittest.main()
