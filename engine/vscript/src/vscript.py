
class TypeError(Exception):
    pass

class NameError(Exception):
    pass

class ArgCountMismatch(Exception):
    pass

class VScriptException(Exception):
    def __init__(self, msg, code):
        self.msg = msg
        self.code = code

    def __str__(self):
        return '%s (%d)' % (self.msg, self.code)
