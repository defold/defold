import sys, subprocess, hashlib, os

class Trace(object):
    def __init__(self, type, ptr, size, back_trace, back_trace_hash):
        self.type = type
        self.ptr = ptr
        self.size = size
        self.back_trace = back_trace
        self.back_trace_hash = back_trace_hash

    def __repr__(self):
        return 'TRACE: back_trace: %s back_trace_hash: %s, size: %d, ptr: %X, type: %s' % (str(self.back_trace), self.back_trace_hash, self.size, self.ptr, str(self.type))

class TraceSummary(object):
    def __init__(self, lst):
        self.nmalloc = 0
        self.nfree = 0
        self.malloc_total = 0
        self.free_total = 0
        self.active_total = 0
        self.back_trace = None

        for t in lst:
            assert self.back_trace == None or self.back_trace == t.back_trace
            self.back_trace = t.back_trace
            if t.type == 'M':
                assert self.nfree == 0
                self.nmalloc += 1
                self.malloc_total += t.size
            elif t.type == 'F':
                assert self.nmalloc == 0
                self.nfree += 1
                self.free_total += t.size
            else:
                assert False

    def __repr__(self):
        return 'TRACESUMMARY: back_trace: %s nmalloc: %d, nfree: %d, malloc_total: %d, free_total: %d' % (str(self.back_trace), self.nmalloc,  self.nfree, self.malloc_total, self.free_total)

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
    if sys.platform == 'darwin':
        p = subprocess.Popen(['atos', '-o', executable], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    else:
        p = subprocess.Popen(['addr2line', '-e', executable], stdin=subprocess.PIPE, stdout=subprocess.PIPE)

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
    active_allocations = {}
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

        if type == 'M':
            active_allocations[ptr] = trace
        elif type == 'F':
            if ptr in active_allocations:
                del(active_allocations[ptr])

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

    for ptr, t in active_allocations.iteritems():
        mem_profile.summary[t.back_trace_hash].active_total += t.size


    return mem_profile

def fmt_memory(x):
    ret = '%d B' % x
    if x > 1024*1024:
        ret = '%d M' % (x / (1024.0*1024.0))
    elif x > 1024:
        ret = '%d K' % (x / (1024.0))
    return ret

if __name__ == '__main__':
    if not os.path.exists(sys.argv[1]):
        print >>sys.stderr, 'Executable %s not found' % sys.argv[1]
        sys.exit(5)

    if not os.path.exists(sys.argv[2]):
        print >>sys.stderr, 'Trace file %s not found' % sys.argv[2]
        sys.exit(5)

    profile = load(sys.argv[2], sys.argv[1])

    lst = profile.summary.values()
    lst.sort(lambda x,y: cmp(y.malloc_total, x.malloc_total))
    active_total = 0
    for s in lst:
        if s.nmalloc > 0:
            active_total += s.active_total

    print '<html>'
    print '<b>Active total: %s</b>' % fmt_memory(active_total)
    print '<p>'
    print '<table border="1">'
    print '<td><b>Backtrace</b></td><td><b>Allocations</b></td><td><b>Total</b></td><td><b>Active</b></td><tr/>'
    for s in lst:
        if s.nmalloc > 0:
            bt = ''
            for x in s.back_trace:
                try:
                    int(x, 16)
                except ValueError:
                    bt += x.split('(')[0] + '<br>\n'

            tot = '%d B' % s.malloc_total
            if s.malloc_total > 1024*1024:
                tot = '%d M' % (s.malloc_total / (1024.0*1024.0))
            elif s.malloc_total > 1024:
                tot = '%d K' % (s.malloc_total / (1024.0))

            active = '%d B' % s.active_total
            if s.active_total > 1024*1024:
                active = '%d M' % (s.active_total / (1024.0*1024.0))
            elif s.active_total > 1024:
                active = '%d K' % (s.active_total / (1024.0))

            print '<td>%s</td><td>%d</td><td>%s</td><td>%s</td><tr/>' % (bt, s.nmalloc, tot, active)

    print '</table>'
    print '</html>'

