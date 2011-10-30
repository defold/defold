from glob import glob
from os.path import splitext, join

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
                       task_gens = {'.c' : c_lang})

def task(**kwargs):
    return set_default(dict(kwargs))

def change_ext(p, input, ext):
    return join(p['bld_dir'], splitext(input)[0] + ext)

def build(p):
    collect_inputs(p)
    create_tasks(p)

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

# Builtin tasks

def c_lang_file(input, output, transform):
    pass

def c_lang(p, input):
    return task(function = c_lang_file,
                inputs = [input],
                outputs = [change_ext(p, input, ".o")])
