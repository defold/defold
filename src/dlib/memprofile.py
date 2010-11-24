import sys, subprocess, hashlib

def load_trace(f):
    pass

class Trace(object):
    def __init__(self, type, ptr, size, back_trace, back_trace_hash):
        self.type = type
        self.ptr = ptr
        self.size = size
        self.back_trace = back_trace
        self.back_trace_hash = back_trace_hash

class TraceSummary(object):
    def __init__(self, lst):
        self.nmalloc = 0
        self.nfree = 0
        self.malloc_total = 0
        self.free_total = 0
        self.back_trace = None

        for t in lst:
            assert self.back_trace == None or self.back_trace == t.back_trace
            self.back_trace = t.back_trace
            if t.type == 'M':
                self.nmalloc += 1
                self.malloc_total += t.size
            elif t.type == 'F':
                self.nfree += 1
                self.free_total += t.size
            else:
                assert False

    def __repr__(self):
        return 'nmalloc: %d, nfree: %d, malloc_total: %d, free_total: %d' % (self.nmalloc,  self.nfree, self.malloc_total, self.free_total)

class MemProfile(object):
    def __init__(self):
        # Address to string
        self.symbol_table = {}
        # Trace to list of calls
        self.traces = {}
        # Trace to allocation summary
        self.summary = {}

def load_symbol_table(addresses, executable):
    symbol_table = {}
    p = subprocess.Popen(['atos', '-o', executable], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    str = ''
    for s in addresses:
        str += "0x%x\n" % s
    stdout, stderr = p.communicate(str)

    text_symbols = stdout.split('\n')
    symbol_table = {}
    for i,s in enumerate(addresses):
        symbol_table[s] = text_symbols[i]

    return symbol_table

def load(trace, executable):
    mem_profile = MemProfile()
    traces = []
    for line in open(trace, 'rb'):
        type, ptr, size, trace = line.split(' ', 3)
        ptr, size = int(ptr, 16), int(size, 16)

        lst = []
        for s in trace.split():
            x = int(s, 16)
            mem_profile.symbol_table[x] = None
            lst.append(int(s, 16))

        h = hashlib.md5(trace).digest()
        trace = Trace(type, ptr, size, lst, h)
        traces.append(trace)

    mem_profile.symbol_table = load_symbol_table(mem_profile.symbol_table.keys(), executable)

    cache = {}
    for t in traces:
        if not t.back_trace_hash in cache:
            lst = [ mem_profile.symbol_table[x] for x in t.back_trace ]
            cache[t.back_trace_hash] = lst
        t.back_trace = cache[t.back_trace_hash]

        lst = mem_profile.traces.get(t.back_trace_hash, [])
        lst.append(t)
        mem_profile.traces[t.back_trace_hash] = lst

    for k, lst in mem_profile.traces.iteritems():
        assert not k in mem_profile.summary
        mem_profile.summary[k] = TraceSummary(lst)

    return mem_profile
