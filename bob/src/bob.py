#! /usr/bin/env python

from __future__ import with_statement

import re, os, stat
from imp import new_module
from os import makedirs, listdir, walk, linesep, pathsep
from os.path import splitext, join, exists, dirname, normpath, isdir, isfile
from hashlib import sha1
from subprocess import Popen, PIPE
import re, logging
import cPickle
import sys
from fnmatch import fnmatch

exposed_vars = ['project', 'task', 'build', 'change_ext', 'make_proto',
                'create_task', 'add_task',  'console_listener', 'null_listener',
                'register', 'find_cmd', 'execute_command', 'supported_exts',
                'log_warning', 'log_error', 'find_sources']

if sys.platform.find('java') != -1:
    # setuptools is typically not available in jython
    # We fake the pkg_resources module. Hopefully this
    # hack will work. Google Protocol Buffers requires
    # pkg_resources
    class pkg_resources(object):
        def declare_namespace(self, x): pass
    sys.modules['pkg_resources'] = pkg_resources()

def do_find_cmd(file_name, path_list):
    for directory in path_list:
        found_file_name = os.path.join(directory,file_name)
        if os.path.exists(found_file_name):
            return found_file_name

def find_cmd(prj, file_name, var):
    path_list = os.environ['PATH'].split(pathsep)
    ret = do_find_cmd(file_name, path_list)
    if not ret: raise Exception('The file %s could not be found' % file_name)
    prj[var] = ret
    return ret

def substitute(string, *dicts):
    lst = [ x for x in string.split(' ') if x != '']
    def repl(m):
        # replace ${FOO} type of pattern
        val = None
        key = m.groups()[0]
        key = key.lower()
        for d in dicts:
            val = d.get(key, val)
        if type(val) == list:
            # We need to be able to split
            # the value into a list later
            # '\0' is unique enough :-)
            val = '\0'.join(val)
        return val

    def repl_index(m):
        # replace ${FOO[0]} type of pattern
        val = None
        key, index = m.groups()
        key = key.lower()
        index = int(index)
        for d in dicts:
            val = d.get(key, val)
        if val:
            return val[index]
        else:
            return None

    new_lst = []
    for x in lst:
        x = re.sub('\${(\w*?)}', repl, x)
        x = re.sub('\${(\w*?)\[(\d+)\]}', repl_index, x)
        x = x.split('\0')
        new_lst.extend(x)
    return new_lst

def exec_script(s):
    m = new_module('bscript')
    # expose function to script module
    for k in exposed_vars:
        setattr(m, k, globals()[k])
    exec(s, m.__dict__)
    return m

def set_default(d, **kwargs):
    for k, v in kwargs.iteritems():
        if not d.has_key(k):
            d[k] = v
    return d

def register(prj, ext, fact_func):
    for e in ext.split(' '):
        prj['task_gens'][e] = fact_func

def find_sources(prj, dir):
    # find_sources is the union of prj['inputs']
    # and files gathered
    sources = set(prj['inputs'])

    # skip special dirs
    skip_dirs = ['.git']

    # first element in bld_dir
    first_bld_dir = os.path.split(prj['bld_dir'])[0]

    for root, dirs, files in walk(dir):
        # check for special directories
        for sd in skip_dirs:
            if sd in dirs: dirs.remove(sd)

        for d in dirs:
            # NOTE: We use normpath to remove leading ./
            # dir is typically '.'
            tmp = normpath(join(root, d))
            # Do not walk down in build directory
            if tmp.startswith(first_bld_dir):
                dirs.remove(d)
                break

        if not root.startswith(prj['bld_dir']):
            for f in files:
                name, ext = splitext(f)
                if name:
                    # NOTE: We use normpath to remove leading ./
                    # dir is typically '.'
                    full = normpath(join(root, f))
                    if ext in prj['task_gens']:
                        sources.add(full)
    prj['inputs'] = list(sources)

def project(**kwargs):
    return set_default(dict(kwargs),
                       bld_dir = 'build',
                       inputs = [],
                       task_gens = {},
                       includes = [])

def supported_exts(prj):
    return prj['task_gens'].keys()

def null_scanner(p, t): return []

def task(**kwargs):
    return set_default(dict(kwargs), dependencies = [],
                                     scanner = null_scanner,
                                     options = [],
                                     name = 'unnamed',
                                     # set if this tasks input is a result of
                                     # another task. could be used to track errors
                                     # back to the original file
                                     product_of = None,
                                     index = -1)

# TODO: Change to more appropriate name. This function both
# change extension *and* change path to build directory.
def change_ext(p, input, ext):
    if input.startswith(p['bld_dir'] + '/'):
        # Do not prepend bld_dir for files
        # in bld_dir, e.g. generated files.
        # TODO: Better solution than this!?
        input = input[len(p['bld_dir'])+1:]
    new = join(p['bld_dir'], splitext(input)[0] + ext)
    dir = dirname(new)
    if not exists(dir): makedirs(dir)
    return join(new)

def null_listener(prj, task, evt): pass

def console_listener(prj, task, evt):
    if evt == 'start':
        print '[%d/%d] \x1b[32m%s: %s -> %s\x1b[0m' % (task['index'] + 1,
                                     len(prj['tasks']),
                                     task['name'],
                                     ', '.join(task['inputs']),
                                     ', '.join(task['outputs']))
    elif evt == 'done':
        info = task['info']
        code = info['code']
        if code != 0:
            print '\x1b[01;91mtask failed'
            print '%s\x1b[0m' % info['stderr']

def build(p, run = True, listener = console_listener):
    p['listener'] = listener
    if not exists(p['bld_dir']):
        makedirs(p['bld_dir'])
    load_state(p)
    create_tasks(p)
    scan(p)
    info_lst = []
    if run:
        info_lst = run_tasks(p)

    save_state(p)
    return info_lst

def add_task(p, tsk):
    assert type(tsk) == dict
    p['tasks'].append(tsk)
    return tsk

def create_task(p, input):
    ext = splitext(input)[1]
    try:
        tg = p['task_gens'][ext]
    except KeyError,e:
        log_warning('No factory found for "%s"' % input)
        return None
    tsk = tg(p, input)
    assert tsk != None
    return tsk

def create_tasks(p):
    p['tasks'] = []
    for input in p['inputs']:
        create_task(p, input)

    for i, tsk in enumerate(p['tasks']):
        tsk['index'] = i

def scan(p):
    deps = set()
    for t in p['tasks']:
        s = t['scanner']
        d = s(p, t)
        deps = deps.union(set(d))
        t['dependencies'] = list(deps)

def sha1_file(prj, name):
    state = prj['state']
    mtime = os.stat(name)[stat.ST_MTIME]
    cached_sig, cached_mtime = state['file_signatures'].get(name, (None, None))

    if cached_sig and mtime == cached_mtime:
        return cached_sig

    with open(name, 'rb') as f:
        d = sha1(f.read()).hexdigest()
    state['file_signatures'][name] = (d, mtime)
    return d

def task_signature(prj, t):
    s = sha1()
    for i in t['inputs']:
        s.update(sha1_file(prj, i))
    for i in t['dependencies']:
        s.update(sha1_file(prj, i))
    s.update(t['function'].func_code.co_code)
    s.update(str(t['options']))
    t['sig'] = s.hexdigest()

def run_tasks(p):
    ret = []
    state = p['state']

    all_outputs = []
    for t in p['tasks']:
        all_outputs.extend(t['outputs'])
    # set of *all* possible output files
    all_outputs = set(all_outputs)

    # the set of all output files generated
    # in this or previous session
    completed_outputs = set()

    # number of task completed. not the same as number of tasks run
    # this number is always incremented apart from when a task
    # is waiting for task(s) generating input to this task
    completed_count = 0

    while completed_count < len(p['tasks']):
        for t in p['tasks']:

            # deps are the task input files generated by another task not yet completed,
            # i.e. "solve" the dependency graph
            deps = set(t['inputs']).intersection(all_outputs).difference(completed_outputs)
            if len(deps) > 0:
                # postpone task. dependent input not yet generated
                continue

            task_signature(p, t)

            completed_count += 1

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
                try:
                    p['listener'](p, t, 'start')
                    t['function'](p, t)
                except Exception, e:
                    from traceback import format_exc
                    info['stderr'] = format_exc(e)
                    info['code'] = 10

            if info['code'] == 0:
                # store new signatures for output files
                for x in t['outputs']:
                    if not exists(x):
                        logging.warn('output file "%s" does not exists', x)
                    else:
                        state['out_to_sig'][x] = t['sig']

            if run:
                p['listener'](p, t, 'done')

            completed_outputs = completed_outputs.union(set(t['outputs']))

    return ret

def load_state(p):
    name = join(p['bld_dir'], 'state')
    if exists(name):
        with open(name, 'rb') as f:
            p['state'] = cPickle.load(f)
    else:
        p['state'] = { 'out_to_sig' : {}, 'file_signatures' : {} }

def save_state(p):
    name = join(p['bld_dir'], 'state')
    with open(name, 'wb') as f:
        cPickle.dump(p['state'], f)

def execute_command(p, t, cmd, shell = False):
    args = substitute(cmd, p, t)
    try:
        if shell:
            real_args = ' '.join(args)
        else:
            real_args = args
        p = Popen(real_args,
                  shell = shell,
                  stdout = PIPE,
                  stderr = PIPE)
        out, err = p.communicate()
        code = p.returncode
    except Exception,e:
        out, err = '', str(e)
        code = 10

    info = t['info']
    info['code'] = code
    info['stdout'] = out
    if code > 0:
        err = 'command failed: %s%s' % (' '.join(args), linesep) + err
    info['stderr'] = err

# Builtin tasks


def make_proto_file(module, msg_type, transform):

    def proto_file(p, t):
        mod = __import__(module)
        # NOTE: We can't use getattr. msg_type could of form "foo.bar"
        msg = eval('mod.' + msg_type)() # Call constructor on message type
        from google.protobuf.text_format import Merge
        with open(t['inputs'][0], 'rb') as in_f:
            Merge(in_f.read(), msg)

        transform(msg)

        with open(t['outputs'][0], 'wb') as out_f:
            out_f.write(msg.SerializeToString())

    return proto_file

def unity_transform(msg): return msg

def make_proto(module, msg_type, transform = unity_transform):
    def proto(p, input):
        ext = splitext(input)[1]
        return add_task(p, task(function = make_proto_file(module, msg_type, transform),
                                name = ext[1:],
                                inputs = [input],
                                outputs = [change_ext(p, input, ext + 'c')]))
    return proto

def log_error(msg):
    print >>sys.stderr, '\x1b[01;91m%s\x1b[0m' % msg

def log_warning(msg):
    print >>sys.stderr, '\x1b[33m%s\x1b[0m' % msg

if __name__ == '__main__':
    if not exists('bscript'):
        print >>sys.stderr, '\x1b[01;91mno bscript file found\x1b[0m'
        sys.exit(5)

    # create a fake bob module so that
    # import bob works in bob_x.py files
    # bob doesn't end with .py and is therefore
    # not importable
    # we could change that and add dir(__file__) to
    # sys.path instead
    bob_mod = new_module('bob')
    for k in exposed_vars:
        setattr(bob_mod, k, globals()[k])
    sys.modules['bob'] = bob_mod

    with open('bscript') as f:
        exec_script(f.read())

