from glob import glob
import re
from os import makedirs, listdir, walk
from os.path import splitext, join, exists, dirname, normpath, isdir, isfile
from hashlib import sha1
from subprocess import Popen, PIPE
import re, logging
import cPickle
import sys
from fnmatch import fnmatch

if sys.platform.find('java') != -1:
    # setuptools is typically not available in jython
    # We fake the pkg_resources module. Hopefully this
    # hack will work. Google Protocol Buffers requires
    # pkg_resources
    class pkg_resources(object):
        def declare_namespace(self, x): pass
    sys.modules['pkg_resources'] = pkg_resources()

glob_pattern = re.compile('[*?[]')

def do_ant_glob(base, elements, recurse):
    '''Do "ant globbing" given a base directory
    and a list of path elements, i.e. a path split by /
    recurse is set to true if ** previously is found
    '''

    def match_single(full, last):
        if not exists(full):
            return []
        elif isdir(full):
            return do_ant_glob(full, elements[1:], recurse)
        elif last:
            return [full]

        return []

    if elements == []:
        # base recursion case: empty list of path elements
        return []
    else:
        # general recursion case. match first element
        # in path element list
        last = len(elements) == 1

        ret = []
        if elements[0] == '**' and last:
            # special case that conflicts with base recursion case 'empty list'
            # last element is '**'. find all files and short circuit
            for root, dirs, files in walk(base):
                ret += [ join(root, x) for x in files ]
            return ret
        elif elements[0] == '**':
            recurse = True
        elif glob_pattern.match(elements[0]):
            lst = listdir(base)
            for f in lst:
                if fnmatch(f, elements[0]):
                    ret += match_single(join(base, f), last)
        else:
            ret = match_single(join(base, elements[0]), last)

        if recurse:
            lst = listdir(base)
            for f in lst:
                if isdir(join(base, f)):
                    if elements[0] == '**':
                        # only remove first element for '**'
                        # otherwise we should just recurse
                        elements = elements[1:]
                    ret += do_ant_glob(join(base, f), elements, True)

        return ret

def ant_glob(inc, excl = ''):
    def do(s):
        s = s.replace('\\', '/')
        elem_lst = s.split('/')
        lst = do_ant_glob('.', elem_lst, False)
        # remove leading ./
        lst = map(normpath, lst)
        return lst

    lst = do(inc)

    if excl:
        excl_lst = do(excl)
        s = set(lst)
        s = s.difference(set(excl_lst))
        lst = list(s)

    return lst

def exec_script(s):
    l = {}
    exec(s, globals(), l)
    return l

def set_default(d, **kwargs):
    for k, v in kwargs.iteritems():
        if not d.has_key(k):
            d[k] = v
    return d

def project(**kwargs):
    return set_default(dict(kwargs),
                       globs = [],
                       task_gens = {'.c' : c_lang},
                       includes = [])

def null_scanner(p, t): return []

def task(**kwargs):
    return set_default(dict(kwargs), dependencies = [],
                                     scanner = null_scanner,
                                     options = [])

def change_ext(p, input, ext):
    new = join(p['bld_dir'], splitext(input)[0] + ext)
    dir = dirname(new)
    if not exists(dir): makedirs(dir)
    return join(new)

def build(p, run = True):
    load_state(p)
    collect_inputs(p)
    create_tasks(p)
    scan(p)
    file_signatures(p)
    task_signatures(p)
    info_lst = []
    if run:
        info_lst = run_tasks(p)

    save_state(p)
    return info_lst

def collect_inputs(p):
    '''Collection input files for project. Input files are collected
    for every glob pattern in the project.globs list'''

    inputs = set()
    for g in p["globs"]:
        inputs = inputs.union(set(glob(g)))
    p['inputs'] = list(inputs)

def create_tasks(p):
    tasks = []
    for input in p['inputs']:
        try:
            tg = p['task_gens'][splitext(input)[1]]
            tasks.append(tg(p, input))
        except KeyError: pass
    p['tasks'] = tasks

def scan(p):
    deps = set()
    for t in p['tasks']:
        s = t['scanner']
        d = s(p, t)
        deps = deps.union(set(d))
        t['dependencies'] = list(deps)

def sha1_file(name):
    f = open(name, 'rb')
    d = sha1(f.read()).hexdigest()
    f.close()
    return d

def file_signatures(p):
    sigs = {}
    for t in p['tasks']:
        for i in p['inputs']:
            sigs[i] = sha1_file(i)
        for i in t['dependencies']:
            sigs[i] = sha1_file(i)
    p['file_signatures'] = sigs

def task_signatures(p):
    sigs = {}
    file_sigs = p['file_signatures']
    for t in p['tasks']:
        s = sha1()
        for i in p['inputs']:
            s.update(file_sigs[i])
        for i in t['dependencies']:
            s.update(file_sigs[i])
        s.update(str(t['options']))
        t['sig'] = s.hexdigest()

def run_tasks(p):
    ret = []
    state = p['state']

    for t in p['tasks']:
        # do all output files exist?
        all_exists = all([ exists(x) for x in t['outputs'] ])

        # compare all task signature. current task signature between previous
        # signature from state on disk
        all_sigs = [ state['out_to_sig'].get(x, None) for x in t['outputs'] ]
        sigs_equal = reduce(lambda x,y: x and (y == t['sig']), all_sigs, True)

        run = not all_exists or not sigs_equal

        info = { 'task' : t,
                 'run' : run,
                 'code' : 0,
                 'stdout' : '',
                 'stderr' : '' }

        t['info'] = info

        ret.append(info)

        code = 0
        if run:
            t['function'](p, t)

        if info['code'] == 0:
            # store new signatures for output files
            for x in t['outputs']:
                if not exists(x):
                    logging.warn('output file "%s" does not exists', x)
                else:
                    state['out_to_sig'][x] = t['sig']

    return ret

def load_state(p):
    name = join(p['bld_dir'], 'state')
    if exists(name):
        f = open(name, 'rb')
        p['state'] = cPickle.load(f)
        f.close()
    else:
        p['state'] = { 'out_to_sig' : {} }

def save_state(p):
    name = join(p['bld_dir'], 'state')
    f = open(name, 'wb')
    cPickle.dump(p['state'], f)
    f.close()

# Builtin tasks

def c_lang_file(p, t):
    args = ['gcc', '-c'] + t['options'] + [t['inputs'][0]] + ['-o', t['outputs'][0]]
    p = Popen(args,
              shell = False,
              stdout = PIPE,
              stderr = PIPE)
    out, err = p.communicate()
    code = p.returncode

    info = t['info']
    info['code'] = code
    info['stdout'] = out
    info['stderr'] = err

header_pattern = re.compile('.*?#include.*?[<"](.+?)[>"].*')
def c_scanner(p, t):
    def find_include(fname, include_paths):
        for i in include_paths:
            full = join(i, fname)
            if exists(full): return normpath(full), i
        return None, None

    def scan(fname, include_paths, scanned):
        if fname in scanned:
            return set()
        scanned.add(fname)
        this_paths = [dirname(fname)] + include_paths

        ret = set()
        f = open(fname)
        for line in f:
            m = header_pattern.match(line)
            if m:
                i = m.groups()[0]
                qual, path = find_include(i, this_paths)
                if qual:
                    ret.add(qual)
                    ret = ret.union(scan(qual, include_paths, scanned))
                else:
                    logging.warn('include "%s" not found in %s', i, this_paths)
        f.close()
        return ret

    deps = set()
    includes = list(p['includes'])
    for i in t['inputs']:
        deps = deps.union(scan(i, includes, set()))
    return list(deps)

def c_lang(p, input):
    options = [ '-I%s' % x for x in p['includes']]
    return task(function = c_lang_file,
                inputs = [input],
                outputs = [change_ext(p, input, '.o')],
                scanner = c_scanner,
                options = options)

def make_proto_file(module, msg_type, transform):

    def proto_file(p, t):
        mod = __import__(module)
        # NOTE: We can't use getattr. msg_type could of form "foo.bar"
        msg = eval('mod.' + msg_type)() # Call constructor on message type
        from google.protobuf.text_format import Merge
        in_f = open(t['inputs'][0], 'rb')
        Merge(in_f.read(), msg)
        in_f.close()

        transform(msg)

        out_f = open(t['outputs'][0], 'wb')
        out_f.write(msg.SerializeToString())
        out_f.close()

    return proto_file

def unity_transform(msg): return msg

def make_proto(module, msg_type, transform = unity_transform):
    def proto(p, input):
        ext = splitext(input)[1]
        return task(function = make_proto_file(module, msg_type, transform),
                    inputs = [input],
                    outputs = [change_ext(p, input, ext + 'c')])
    return proto
