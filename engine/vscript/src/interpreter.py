import sys
import vscript
import vscript_ddf_pb2
from vscript_ddf_pb2 import VisualScriptFile, PrototypeFile
from google.protobuf import text_format

BUILTIN_PROTOTYPES = """
prototypes: {
    name: "pow"

    return_type {
        basic_type: NUMBER
    }

    arguments {
        basic_type: NUMBER
    }

    arguments {
        basic_type: NUMBER
    }
}
"""

BUILTINS = { }

def setup_builtins():
    prototype_file = PrototypeFile()
    text_format.Merge(BUILTIN_PROTOTYPES, prototype_file)
    for proto in prototype_file.prototypes:
        BUILTINS[proto.name] = proto

setup_builtins()

class Visitor(object):
    def __init__(self, source):
        self.script = VisualScriptFile()
        text_format.Merge(source, self.script)
        self.blocks = {}
        for block in self.script.blocks:
            if block.id:
                self.blocks[block.id] = block

    def visit_block(self, block):
        for stmt in block.statements:
            self.visit_statement(stmt)

    # Generic statement dispatch
    def visit_statement(self, stmt):
        name = vscript_ddf_pb2._STATEMENTTYPE.values_by_number[stmt.type].name
        name = name.lower()
        method = getattr(self, 'visit_%s' % name)
        method(stmt)

    # Generic expression dispatch
    def visit_expression(self, expr):
        name = vscript_ddf_pb2._EXPRESSIONTYPE.values_by_number[expr.type].name
        name = name.lower()
        method = getattr(self, 'visit_%s' % name)
        method(expr)

class Value(object):
    def __init__(self, type, value):
        self.type = type
        self.value = value

def to_python_value(type, value):
    if type.basic_type == vscript_ddf_pb2.NUMBER:
        return value.float_value
    elif type.basic_type == vscript_ddf_pb2.STRING:
        return value.string_value
    else:
        assert False

class CheckInterpreter(Visitor):
    def __init__(self, source):
        Visitor.__init__(self, source)

        # Run this to check that the message is complete. Better way?
        self.script.SerializeToString()

        self.globals = {}
        for var in self.script.globals:
            self.globals[var.name] = var.type

        self.stack = []

        self.bool_type = vscript_ddf_pb2.Type()
        self.bool_type.basic_type = vscript_ddf_pb2.BOOL

    def get_global(self, name):
        return self.globals[name]

    def push(self, value):
        self.stack.insert(0, value)

    def pop(self):
        return self.stack.pop(0)

    # Statements
    def visit_if(self, stmt):
        self.visit_expression(stmt.expression)
        cond = self.pop()
        for stmt in stmt.statements:
            self.visit_statement(stmt)

    # Statements
    def visit_if_else(self, stmt):
        self.visit_expression(stmt.expression)
        cond = self.pop()

        for stmt in stmt.statements:
            self.visit_statement(stmt)

        for stmt in stmt.else_statements:
            self.visit_statement(stmt)

    def visit_set_variable(self, stmt):
        self.visit_expression(stmt.expression)

        if not self.globals.has_key(stmt.name):
            raise vscript.NameError('%s not declared' % stmt.name)

        t = self.pop()
        if t != self.globals[stmt.name]:
            raise vscript.TypeError()

    def visit_print(self, stmt):
        self.visit_expression(stmt.expression)
        self.pop()

    # Expressions
    def do_binary_number(self, expr):
        self.visit_expression(expr.expressions[0])
        self.visit_expression(expr.expressions[1])
        t2, t1 = self.pop(), self.pop()
        if t1.basic_type != vscript_ddf_pb2.NUMBER or t2.basic_type != vscript_ddf_pb2.NUMBER:
            raise vscript.TypeError
        self.push(t1)

    def visit_add(self, expr): self.do_binary_number(expr)
    def visit_sub(self, expr): self.do_binary_number(expr)
    def visit_mul(self, expr): self.do_binary_number(expr)
    def visit_div(self, expr): self.do_binary_number(expr)

    def do_binary_cond(self, expr):
        self.visit_expression(expr.expressions[0])
        self.visit_expression(expr.expressions[1])
        t2, t1 = self.pop(), self.pop()
        if t1.basic_type != vscript_ddf_pb2.NUMBER or t2.basic_type != vscript_ddf_pb2.NUMBER:
            raise vscript.TypeError
        self.push(self.bool_type)

    def visit_lt(self, expr): self.do_binary_cond(expr)
    def visit_le(self, expr): self.do_binary_cond(expr)
    def visit_eq(self, expr): self.do_binary_cond(expr)
    def visit_ge(self, expr): self.do_binary_cond(expr)
    def visit_gt(self, expr): self.do_binary_cond(expr)

    def visit_constant(self, expr):
        if expr.constant.type.basic_type == vscript_ddf_pb2.NUMBER:
            self.push(expr.constant.type)
        elif expr.constant.type.basic_type == vscript_ddf_pb2.STRING:
            self.push(expr.constant.type)
        else:
            assert False

    def visit_load_variable(self, expr):
        type = self.get_global(expr.name)
        self.push(type)

    def visit_invoke(self, stmt):
        block = self.blocks[stmt.name]
        self.visit_block(block)

    def visit_invoke_expr(self, expr):
        proto = BUILTINS[expr.name]

        if len(expr.expressions) != len(proto.arguments):
            raise vscript.ArgCountMismatch()

        args = []
        for e in expr.expressions:
            self.visit_expression(e)
            args.insert(0, self.pop())

        for i, arg in enumerate(proto.arguments):
            if arg != args[i]:
                raise vscript.TypeError()

        self.push(proto.return_type)

    def invoke(self, procedure_name):
        block = self.blocks[procedure_name]
        self.visit_block(block)

class Interpreter(Visitor):

    def __init__(self, source):
        Visitor.__init__(self, source)

        # Run this to check that the message is complete. Better way?
        self.script.SerializeToString()

        self.globals = {}
        for var in self.script.globals:
            self.globals[var.name] = Value(var.type, to_python_value(var.type, var.initial_value))

        self.stack = []

    def function_pow(self):
        v2, v1 = self.pop(), self.pop()
        value = v1 ** v2
        self.push(value)

    def get_global(self, name):
        return self.globals[name].value

    def set_global(self, name, value):
        if not self.globals.has_key(name):
            raise vscript.NameError('Global %s not declared' % name)

        self.globals[name].value = value

    def push(self, value):
        self.stack.insert(0, value)

    def pop(self):
        return self.stack.pop(0)

    # Statements
    def visit_if(self, stmt):
        self.visit_expression(stmt.expression)
        cond = self.pop()
        if cond:
            for stmt in stmt.statements:
                self.visit_statement(stmt)

    # Statements
    def visit_if_else(self, stmt):
        self.visit_expression(stmt.expression)
        cond = self.pop()
        if cond:
            for stmt in stmt.statements:
                self.visit_statement(stmt)
        else:
            for stmt in stmt.else_statements:
                self.visit_statement(stmt)

    def visit_set_variable(self, stmt):
        self.visit_expression(stmt.expression)
        self.set_global(stmt.name, self.pop())

    def visit_print(self, stmt):
        self.visit_expression(stmt.expression)
        print self.pop()

    # Expressions
    def do_binary(self, expr, func):
        self.visit_expression(expr.expressions[0])
        self.visit_expression(expr.expressions[1])
        v2, v1 = self.pop(), self.pop()
        self.push(func(v1, v2))

    def visit_add(self, expr):
        self.do_binary(expr, lambda x, y: x + y)

    def visit_sub(self, expr):
        self.do_binary(expr, lambda x, y: x - y)

    def visit_mul(self, expr):
        self.do_binary(expr, lambda x, y: x * y)

    def visit_div(self, expr):
        self.do_binary(expr, lambda x, y: x / y)

    def visit_lt(self, expr):
        self.do_binary(expr, lambda x, y: x < y)

    def visit_le(self, expr):
        self.do_binary(expr, lambda x, y: x <= y)

    def visit_eq(self, expr):
        self.do_binary(expr, lambda x, y: x == y)

    def visit_ge(self, expr):
        self.do_binary(expr, lambda x, y: x >= y)

    def visit_gt(self, expr):
        self.do_binary(expr, lambda x, y: x > y)

    def visit_constant(self, expr):
        if expr.constant.type.basic_type == vscript_ddf_pb2.NUMBER:
            self.push(expr.constant.float_value)
        elif expr.constant.type.basic_type == vscript_ddf_pb2.STRING:
            self.push(expr.constant.string_value)
        else:
            assert False

    def visit_load_variable(self, expr):
        value = self.get_global(expr.name)
        self.push(value)

    def visit_invoke(self, stmt):
        block = self.blocks[stmt.name]
        self.visit_block(block)

    def visit_invoke_expr(self, expr):
        for e in expr.expressions:
            self.visit_expression(e)

        func = getattr(self, 'function_%s' % expr.name)
        func()

    def invoke(self, procedure_name):
        block = self.blocks[procedure_name]
        self.visit_block(block)

    def dispatch(self, event_name, event_id):
        for block in self.script.blocks:
            if block.event_name == event_name and block.event_id == event_id:
                self.visit_block(block)

if __name__ == '__main__':
    source = open(sys.argv[1], 'r').read()
    interpreter = Interpreter(source)
    interpreter.run()
