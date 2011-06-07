import unittest
from google.protobuf import text_format

from interpreter import Interpreter
import sys, os

class TestVScript(unittest.TestCase):

    def parse(self, name):
        script_source = open('src/test/data/%s.vscript' % name, 'r').read()
        return Interpreter(script_source)

    def test_print(self):
        interpreter = self.parse('test_print')
        interpreter.invoke('main')

    def test_set_variable(self):
        interpreter = self.parse('test_set_variable')
        interpreter.invoke('main')
        self.assertEquals(interpreter.get_global('x'), 123)

    def test_get_variable(self):
        interpreter = self.parse('test_get_variable')
        interpreter.set_global('a_constant', 10000)
        interpreter.invoke('main')
        self.assertEquals(interpreter.get_global('x'), 10000)

    def test_constants(self):
        interpreter = self.parse('test_constants')
        interpreter.invoke('main')
        self.assertEquals(interpreter.get_global('number'), 123)
        self.assertEquals(interpreter.get_global('string'), 'hello')

    def test_if1(self):
        interpreter = self.parse('test_if1')
        interpreter.invoke('main')
        self.assertEquals(interpreter.get_global('x'), 1010)

    def test_if_else1(self):
        interpreter = self.parse('test_if_else1')
        interpreter.invoke('main')
        self.assertEquals(interpreter.get_global('x'), 2)

    def test_invoke_statement(self):
        interpreter = self.parse('test_invoke_statement')
        interpreter.invoke('main')
        self.assertEquals(interpreter.get_global('x'), 123)

    def test_invoke_expression(self):
        interpreter = self.parse('test_invoke_expression')
        interpreter.invoke('main')
        self.assertEquals(interpreter.get_global('x'), 16)

    def test_handler(self):
        interpreter = self.parse('test_handler')
        interpreter.dispatch('input', 'jump')
        self.assertEquals(interpreter.get_global('x'), 123)

        interpreter.dispatch('input', 'run')
        self.assertEquals(interpreter.get_global('x'), 456)


if __name__ == '__main__':
    unittest.main()
