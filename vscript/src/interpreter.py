import sys
from vscript_ddf_pb2 import VisualScript, STATEMENT_TYPE_PRINT, EXPRESSION_TYPE_ADD, EXPRESSION_TYPE_CONSTANT
from google.protobuf import text_format


class Interpreter(object):

    def __init__(self, source):
        self.script = VisualScript()
        text_format.Merge(source, self.script)
        self.stack = []

    def push(self, value):
        self.stack.insert(0, value)

    def pop(self):
        return self.stack[0]

    def visit_Node(self, node):
        for stmt in node.statements:
            self.visit_Statement(stmt)

    def visit_StmtPrint(self, stmt):
        self.visit_Expression(stmt.expression)
        print self.pop()

    def visit_Statement(self, stmt):
        self.statement_methods[stmt.type](self, stmt)

    def visit_Expression(self, expr):
        self.expression_methods[expr.type](self, expr)

    def visit_ExpressionAdd(self, expr):
        self.visit_Expression(expr.expressions[0])
        self.visit_Expression(expr.expressions[1])
        v2, v1 = self.pop(), self.pop()
        self.push(v1 + v2)

    def visit_ExpressionConstant(self, expr):
        self.push(expr.constant.float_value)

    def run(self):
        for node in self.script.nodes:
            self.visit_Node(node)

    statement_methods = { STATEMENT_TYPE_PRINT : visit_StmtPrint }
    expression_methods = { EXPRESSION_TYPE_ADD : visit_ExpressionAdd,
                           EXPRESSION_TYPE_CONSTANT : visit_ExpressionConstant }


if __name__ == '__main__':
    source = open(sys.argv[1], 'r').read()
    interpreter = Interpreter(source)
    interpreter.run()
