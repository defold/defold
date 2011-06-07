import unittest
from google.protobuf import text_format

import vscript
from interpreter import Interpreter, CheckInterpreter
import sys, os

class TestVScriptPositive(unittest.TestCase):

    def parse_invoke(self, name, globals = {}):
        script_source = open('src/test/data/positive/%s.vscript' % name, 'r').read()

        check_interpreter = CheckInterpreter(script_source)
        check_interpreter.invoke('main')

        interpreter = Interpreter(script_source)
        for k,v in globals.iteritems():
            interpreter.set_global(k, v)

        interpreter.invoke('main')
        return interpreter

    def test_print(self):
        interpreter = self.parse_invoke('test_print')

    def test_set_variable(self):
        interpreter = self.parse_invoke('test_set_variable')
        self.assertEquals(interpreter.get_global('x'), 123)

    def test_get_variable(self):
        interpreter = self.parse_invoke('test_get_variable', globals = { 'a_constant' : 10000 } )
        self.assertEquals(interpreter.get_global('x'), 10000)

    def test_constants(self):
        interpreter = self.parse_invoke('test_constants')
        self.assertEquals(interpreter.get_global('number'), 123)
        self.assertEquals(interpreter.get_global('string'), 'hello')

    def test_if1(self):
        interpreter = self.parse_invoke('test_if1')
        self.assertEquals(interpreter.get_global('x'), 1010)

    def test_if_else1(self):
        interpreter = self.parse_invoke('test_if_else1')
        self.assertEquals(interpreter.get_global('x'), 2)

    def test_invoke_statement(self):
        interpreter = self.parse_invoke('test_invoke_statement')
        self.assertEquals(interpreter.get_global('x'), 123)

    def test_invoke_expression(self):
        interpreter = self.parse_invoke('test_invoke_expression')
        self.assertEquals(interpreter.get_global('x'), 16)

    def test_handler(self):
        script_source = open('src/test/data/positive/%s.vscript' % 'test_handler', 'r').read()
        interpreter = Interpreter(script_source)

        interpreter.dispatch('input', 'jump')
        self.assertEquals(interpreter.get_global('x'), 123)

        interpreter.dispatch('input', 'run')
        self.assertEquals(interpreter.get_global('x'), 456)

class TestVScriptNegative(unittest.TestCase):

    def parse_test(self, name, exception):
        script_source = open('src/test/data/negative/%s.vscript' % name, 'r').read()
        def f():
            interpreter = CheckInterpreter(script_source)
            interpreter.invoke('main')

        return CheckInterpreter(script_source)

    def test_type_error_binary1(self):
        self.parse_test('test_type_error_binary1', vscript.TypeError)

    def test_type_error_cond1(self):
        self.parse_test('test_type_error_cond1', vscript.TypeError)

    def test_type_error_invoke_expression1(self):
        self.parse_test('test_type_error_invoke_expression1', vscript.TypeError)

    def test_arg_count_mismatch1(self):
        self.parse_test('test_arg_count_mismatch1', vscript.ArgCountMismatch)

    def test_type_error_set_variable(self):
        self.parse_test('test_type_error_set_variable', vscript.TypeError)

if __name__ == '__main__':
    unittest.main()
