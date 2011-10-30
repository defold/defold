from glob import glob
from os import makedirs
from os.path import splitext, join, exists, dirname, normpath
from hashlib import sha1
from subprocess import Popen, PIPE
import re, logging
import cPickle
import sys

if sys.platform.find('java') != -1:
    # setuptools is typically not available in jython
    # We fake the pkg_resources module. Hopefully this
    # hack will work. Google Protocol Buffers requires
    # pkg_resources
    class pkg_resources(object):
        def declare_namespace(self, x): pass
    sys.modules['pkg_resources'] = pkg_resources()

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
            if header_pattern.match(line):
                i = header_pattern.match(line).groups()[0]
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
                outputs = [change_ext(p, input, ".o")],
                scanner = c_scanner,
                options = options)
