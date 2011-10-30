from glob import glob

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
    return set_default(dict(kwargs), globs = [])

def build(p):
    collect_inputs(p)

def collect_inputs(p):
    '''Collection input files for project. Input files are collected
    for every glob pattern in the project.globs list'''

    inputs = set()
    for g in p["globs"]:
        inputs = inputs.union(set(glob(g)))
    p['inputs'] = list(inputs)
