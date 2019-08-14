#!/usr/bin/env python
#

import re

stags = [  'C', 'Cid', 'Char', 'Lid', 'M', 'N', 'Produc', 'Q', 'Rw', 'See', 'St', 'T', 'bnfNter',
            'bnfopt', 'bnfrep', 'bnfter', 'def', 'defid', 'emph', 'id', 'idx', 'index', 'num',
            'Lidx', 'sp', 'item',
            'producname', 'producbody', 'rep', 'refsec', 'see', 'seeC', 'seeF', 'title', 'x',
            'nil', 'false', 'true', 'En', 'Em', 'Cdots', 'Or', 'Open', 'Close',
            'leq', 'At', 'pi', 'OrNL',
            'format', 'apii' ]

types = [ 'nil', 'false', 'true' ]

stags_throw = [ 'C', 'index', 'see', 'seeC', 'seeF', 'format' ]

ctags = [   'APIEntry', 'LibEntry', 'Produc', 'description', 'itemize', 'manual',
            'sect1', 'sect2', 'sect3', 'sect4', 'verbatim' ]

ctags_throw = [ ]

# Function brief lookup. From https://www.gammon.com.au/scripts/doc.php
briefs = { "assert": "asserts that condition is not nil and not false", "collectgarbage": "collects garbage", "dofile": "executes a Lua file", "error": "raises an error message", "gcinfo": "returns amount of dynamic memory in use", "getfenv": "returns the current environment table", "getmetatable": "returns the metatable for the object", "ipairs": "iterates over a numerically keyed table", "load": "loads a chunk by calling a function repeatedly", "loadfile": "loads a Lua file and parses it", "loadlib": "loads a DLL (obsolete in Lua 5.1)", "loadstring": "compiles a string of Lua code", "module": "creates a Lua module", "next": "returns next key / value pair in a table", "pairs": "traverse all items in a table", "pcall": "calls a function in protected mode", "print": "prints its arguments", "rawequal": "compares two values for equality without invoking metamethods", "rawget": "gets the value of a table item without invoking metamethods", "rawset": "sets the value of a table item without invoking metamethods", "require": "loads a module", "select": "returns items in a list", "setfenv": "sets a function's environment", "setmetatable": "sets the metatable for a table", "tonumber": "converts a string (of the given base) to a number", "tostring": "converts its argument to a string", "type": "returns the type of a variable", "unpack": "unpacks a table into individual items", "xpcall": "calls a function with a custom error handler", "coroutine.create": "creates a new coroutine thread", "coroutine.resume": "start or resume a thread", "coroutine.running": "returns the running coroutine", "coroutine.status": "returns the status of a thread", "coroutine.wrap": "creates a thread and returns a function to resume it", "coroutine.yield": "yields execution of thread back to the caller", "bit.ashr": "shifts a number right, preserving sign", "bit.band": "bitwise 'and'", "bit.bor": "bitwise 'or'", "bit.clear": "clears one or more bits", "bit.mod": "bitwise 'modulus' (remainder after integer divide)", "bit.neg": "bitwise 'negate' (ones complement)", "bit.shl": "shifts a number left", "bit.shr": "shifts a number right", "bit.test": "bitwise 'test'", "bit.tonumber": "convert a string into a number", "bit.tostring": "convert a number into a string", "bit.xor": "bitwise 'exclusive or'", "debug.debug": "enters interactive debugging", "debug.getfenv": "returns the environment of an object", "debug.gethook": "returns the current hook settings", "debug.getinfo": "returns a table with information about a function", "debug.getlocal": "returns name and value of a local variable", "debug.getmetatable": "returns the metatable of the given object", "debug.getregistry": "returns the registry table", "debug.getupvalue": "returns the name and value of an upvalue", "debug.setfenv": "sets the environment of an object", "debug.sethook": "sets a debug hook function", "debug.setlocal": "sets the value of the local variable", "debug.setmetatable": "sets the metatable for an object", "debug.setupvalue": "sets an upvalue for a function", "debug.traceback": "returns a string with a traceback of the stack call", "file:close": "closes a file", "file:flush": "flushes outstanding data to disk", "file:lines": "returns an iterator function for reading the file line-by-line", "file:read": "reads the file according to the specified formats", "file:seek": "sets and gets the current file position", "file:setvbuf": "sets the buffering mode for an output file", "file:write": "writes to a file", "io.close": "closes a file", "io.flush": "flushes outstanding data to disk for the default output file", "io.input": "opens filename for input in text mode", "io.lines": "returns an iterator function for reading a named file line-by-line", "io.open": "opens a file", "io.output": "opens a file for output", "io.popen": "creates a pipe and executes a command", "io.read": "reads from the default input file", "io.stderr": "file handle for standard error file", "io.stdin": "file handle for standard input file", "io.stdout": "file handle for standard output file", "io.tmpfile": "returns a handle to a temporary file", "io.type": "returns type of file handle", "io.write": "writes to the default output file", "math.abs": "absolute value", "math.acos": "arc cosine", "math.asin": "arc sine", "math.atan": "arc tangent", "math.atan2": "arc tangent of v1/v2", "math.ceil": "next higher integer value", "math.cos": "cosine", "math.cosh": "hyperbolic cosine", "math.deg": "convert from radians to degrees", "math.exp": "raises e to a power", "math.floor": "next smaller integer value", "math.fmod": "the modulus (remainder) of doing: v1 / v2", "math.frexp": "break number into mantissa and exponent", "math.huge": "a huge value", "math.ldexp": "compute m* 2^n", "math.log": "natural log", "math.log10": "log to the base 10", "math.max": "the highest of one or more numbers", "math.min": "the lowest of one or more numbers", "math.modf": "returns the integral and fractional part of its argument", "math.pi": "the value of pi", "math.pow": "raise a number to a power", "math.rad": "convert degrees to radians", "math.random": "generate a random number", "math.randomseed": "seeds the random number generator", "math.sin": "sine", "math.sinh": "hyperbolic sine", "math.sqrt": "square root", "math.tan": "tangent", "math.tanh": "hyperbolic tangent", "os.clock": "amount of elapsed/CPU time used (depending on OS)", "os.date": "formats a date/time string", "os.difftime": "calculates a time difference in seconds", "os.execute": "executes an operating system command", "os.exit": "attempts to terminate the process", "os.getenv": "returns an operating system environment variable", "os.remove": "deletes a file", "os.rename": "renames a file", "os.setlocale": "sets the current locale to the supplied locale", "os.time": "returns the current time or calculates the time in seconds from a table", "os.tmpname": "returns a name for a temporary file", "package.config": "package configuration string", "package.cpath": "search path used for loading DLLs using the ```require``` function", "package.loaded": "table of loaded packages", "package.loaders": "table of package loaders", "package.loadlib": "loads a dynamic link library (DLL)", "package.path": "search path used for loading Lua code using the ```require``` function", "package.preload": "a table of special function loaders", "package.seeall": "sets a metatable for the module so it can see global variables", "string.byte": "converts a character into its ASCII (decimal) equivalent", "string.char": "converts ASCII codes into their equivalent characters", "string.dump": "converts a function into binary", "string.find": "searches a string for a pattern", "string.format": "formats a string", "string.gfind": "iterate over a string (obsolete in Lua 5.1)", "string.gmatch": "iterate over a string", "string.gsub": "substitute strings inside another string", "string.len": "return the length of a string", "string.lower": "converts a string to lower-case", "string.match": "searches a string for a pattern", "string.rep": "returns repeated copies of a string", "string.reverse": "reverses the order of characters in a string", "string.sub": "returns a substring of a string", "string.upper": "converts a string to upper-case", "table.concat": "concatenates table items together into a string", "table.foreach": "applies a function to each item in a table", "table.foreachi": "applies a function to each item in a numerically-keyed table", "table.getn": "returns the size of a numerically-keyed table", "table.insert": "inserts a new item into a numerically-keyed table", "table.maxn": "returns the highest numeric key in the table", "table.remove": "removes an item from a numerically-keyed table", "table.setn": "sets the size of a table (obsolete)", "table.sort": "Sorts a table",
    "_G": "global variable that holds the global environment",
    "_VERSION": "global variable containing the running Lua version",
 }

libentries = {}

def stag(string):
    s = re.search("@\w+(\W)", string)
    i = s.start(1)
    tag = string[1:i]
    if tag not in stags:
        return None, 0
    pre = ''
    post = ''
    pos = i + 1

    # Do this tag have an id?
    m = re.match('@\w+{(.+?)[|]', string)
    id = None
    if m:
        id, consumed = convertstring(m.group(1))
        pos = pos + consumed + 1

    if tag in types:
        return '[type: %s]' % (tag), len(tag) + 1
    elif tag == 'En':
        return '-', 3
    elif tag == 'Em':
        return '-', 3
    elif tag == 'Cdots':
        return '...', 6
    elif tag == 'Or':
        return 'or', 3
    elif tag == 'Open':
        return '[', 5
    elif tag == 'Close':
        return ']', 6
    elif tag == 'leq':
        return '<=', 4
    elif tag == 'At':
        return '@', 3
    elif tag == 'pi':
        return 'PI', 3
    elif tag == 'OrNL':
        return '|', 5
    elif tag == 'emph':
        pre = '<em>'
        post = '</em>'
    elif tag == 'St':
        pre = '"'
        post = '"'
    elif tag == 'Q':
        pre = '"'
        post = '"'
    elif tag == 'Char':
        pre = "'"
        post = "'"
    elif tag == 'sp':
        pre = "<sup>"
        post = "</sup>"
    elif tag == 'T':
        pre = "`"
        post = "`"
    elif tag == 'item':
        if id:
            pre = "%s\n: " % (id)
            post = '\n'
        else:
            pre = '- '
            post = ''

    ret, consumed = convertstring(string[pos:], stop_at='inline')
    if tag and ret[0] == '\n':
        ret = ret[1:]

    pos = pos + consumed

    # Throw away tags
    if tag in stags_throw:
        ret = ''

    return pre + ret + post, pos


def libparams(str):
    ps = str.split(' ')
    optdepth = 0
    r = []
    for p in ps:
        n = re.sub('[,\[\]]', '', p)
        optdepth += p.count('[')
        if n and optdepth > 0:
            r.append("[%s]" % n)
        elif n:
            r.append("%s" % n)
        optdepth -= p.count(']')

    return r


def libfunc(str):
    m = re.match("([a-zA-Z0-9_.:]+) [(](.*?)[)]", str)
    if m:
        func = m.group(1)
        ps = libparams(m.group(2))
        return func, ps
    else:
        return str, []


def ctag(string):
    s = re.search("^@\w+(\W)", string)
    i = s.start(1)
    tag = string[1:i]
    if tag not in ctags:
        return None, 0
    pre = ''
    post = ''
    pos = i + 1

    # Do this tag have an id?
    m = re.match('@\w+{(.+?)[|]', string)
    id = None
    if m:
        id, consumed = convertstring(m.group(1))
        pos = pos + consumed + 1

    if tag == 'verbatim':
        pre = '\n\n```lua\n'
        post = '```\n\n'
    elif tag == 'itemize':
        pre = '\n\n'
        post = '\n\n'
    elif tag == 'description':
        pre = '\n\n'
        post = '\n\n'

    ret, consumed = convertstring(string[pos:], stop_at='separate')
    if tag and ret[0] == '\n':
        ret = ret[1:]

    pos = pos + consumed

    # Throw away tags
    if tag in ctags_throw:
        ret = ''

    # Libentry. Store a docstring
    if tag == 'LibEntry':
        name, params = libfunc(id)
        namespace = "base"
        m = re.match("(\w+)[.]", name)
        if m:
            namespace = m.group(1)
        if name[0:5] == "file:":
            namespace = "io"

        entry = ""
        entry += "/*# %s\n" % briefs[name]
        for s in ret.split('\n'):
            entry += " * %s\n" % s
        if name[0] == "_":
            entry += " * @variable\n"
            entry += " * @name %s\n" % name
        else:
            entry += " * @name %s\n" % name
            for p in params:
                entry += " * @param %s\n" % p
        entry += " */\n"
        if not namespace in libentries:
            libentries[namespace] = []

        libentries[namespace].append(entry)

    return pre + ret + post, pos

def convertstring(string, stop_at='nothing'):
    pos = 0
    ret = ''
    while pos < len(string):
        c = string[pos]
        if c == '@':
            nested, consumed = stag(string[pos:])
            if nested != None:
                ret = ret + nested
                pos = pos + consumed
                continue

            nested, consumed = ctag(string[pos:])
            if nested != None:
                ret = ret + nested
                pos = pos + consumed
                continue

            print("ERROR: unknown tag in '%s'" % (string[pos:]))
            break
        elif stop_at == 'inline' and c == '}':
            pos = pos + 1
            break
        elif stop_at == 'separate' and string[pos-1:pos+2] == '\n}\n':
            pos = pos + 2
            break
        else:
            ret = ret + string[pos]
            pos = pos + 1
    return ret, pos

file = "scripts/manual.of"
f = open(file, "r")
content = f.read()

res, _ = convertstring(content)
for ns, ss in libentries.items():
    f = open("src/lua_%s.doc_h" % ns, "w")
    f.write("/*# Lua %s standard library\n" % ns)
    f.write(" *\n")
    f.write(" * Documentation for the Lua %s standard library.\n" %ns)
    f.write(" *\n")
    f.write(" * From [Lua 5.1 Reference Manual](https://www.lua.org/manual/5.1/)\n")
    f.write(" * by Roberto Ierusalimschy, Luiz Henrique de Figueiredo, Waldemar Celes.\n")
    f.write(" *\n")
    f.write(" * Copyright &copy; 2006-2012 Lua.org, PUC-Rio.\n")
    f.write(" *\n")
    f.write(" * Freely available under the terms of the [Lua license](https://www.lua.org/license.html).\n")
    f.write(" *\n")
    f.write(" * @document\n")
    f.write(" * @name %s\n" % ns.capitalize())
    f.write(" * @namespace %s\n"% ns)
    f.write(" */\n\n")
    for s in ss:
        f.write(s)
        f.write("\n")
