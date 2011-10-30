from glob import glob
from os.path import splitext, join, exists, dirname, normpath
import re, logging

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
                                     scanner = null_scanner)

def change_ext(p, input, ext):
    return join(p['bld_dir'], splitext(input)[0] + ext)

def build(p):
    collect_inputs(p)
    create_tasks(p)
    scan(p)

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

# Builtin tasks

def c_lang_file(input, output, transform):
    pass

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
    return task(function = c_lang_file,
                inputs = [input],
                outputs = [change_ext(p, input, ".o")],
                scanner = c_scanner)
