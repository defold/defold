#! /usr/bin/env python

from __future__ import with_statement

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
    from imp import new_module
    m = new_module('bscript')
    # expose function to script module
    for k in ['project', 'task', 'build', 'change_ext', 'make_proto', 'create_task', 'add_task', 'console_listener', 'null_listener']:
        setattr(m, k, globals()[k])
    exec(s, m.__dict__)
    return m

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
                                     options = [],
                                     name = 'unnamed',
                                     # set if this tasks input is a result of
                                     # another task. could be used to track errors
                                     # back to the original file
                                     product_of = None,
                                     index = -1)

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

def null_listener(prj, task): pass

def console_listener(prj, task):
    print '[%d/%d] \x1b[33m%s: %s -> %s\x1b[0m' % (task['index'] + 1,
                                 len(prj['tasks']),
                                 task['name'],
                                 ', '.join(task['inputs']),
                                 ', '.join(task['outputs']))
    info = task['info']
    code = info['code']
    if code != 0:
        print '\x1b[01;91mtask failed'
        print '%s\x1b[0m' % info['stderr']

def build(p, run = True, listener = console_listener):
    p['listener'] = listener
    load_state(p)
    collect_inputs(p)
    create_tasks(p)
    scan(p)
    file_signatures(p)
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
        inputs = inputs.union(set(ant_glob(g)))
    inputs = filter(lambda x: not x.startswith('%s/' % p['bld_dir']), inputs)
    p['inputs'] = list(inputs)

def add_task(p, tsk):
    p['tasks'].append(tsk)
    return tsk

def create_task(p, input):
    ext = splitext(input)[1]
    try:
        tg = p['task_gens'][ext]
    except KeyError,e:
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

def sha1_file(name):
    with open(name, 'rb') as f:
        d = sha1(f.read()).hexdigest()
    return d

def file_signatures(p):
    sigs = {}
    for t in p['tasks']:
        for i in p['inputs']:
            sigs[i] = sha1_file(i)
        for i in t['dependencies']:
            sigs[i] = sha1_file(i)
    p['file_signatures'] = sigs

def task_signature(p, t):
    file_sigs = p['file_signatures']
    s = sha1()
    for i in t['inputs']:
        s.update(file_sigs[i])
    for i in t['dependencies']:
        s.update(file_sigs[i])
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
                        p['file_signatures'][x] = sha1_file(x)
                        state['out_to_sig'][x] = t['sig']

            if run:
                p['listener'](p, t)

            completed_outputs = completed_outputs.union(set(t['outputs']))

    return ret

def load_state(p):
    name = join(p['bld_dir'], 'state')
    if exists(name):
        with open(name, 'rb') as f:
            p['state'] = cPickle.load(f)
    else:
        p['state'] = { 'out_to_sig' : {} }

def save_state(p):
    name = join(p['bld_dir'], 'state')
    with open(name, 'wb') as f:
        cPickle.dump(p['state'], f)

def execute_command(p, t, cmd):
    args = substitute(cmd, p, t)
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

# Builtin tasks

def c_lang_file(p, t):
    execute_command(p, t, '${CC} -c ${OPTIONS} ${INPUTS[0]} -o ${OUTPUTS[0]}')

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
        with open(fname) as f:
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
        return ret

    deps = set()
    includes = list(p['includes'])
    for i in t['inputs']:
        deps = deps.union(scan(i, includes, set()))
    return list(deps)

def c_lang(p, input):
    options = [ '-I%s' % x for x in p['includes']]
    return add_task(p, task(function = c_lang_file,
                            cc = 'gcc',
                            name = 'c',
                            inputs = [input],
                            outputs = [change_ext(p, input, '.o')],
                            scanner = c_scanner,
                            options = options))

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
                                name = ext,
                                inputs = [input],
                                outputs = [change_ext(p, input, ext + 'c')]))
    return proto

if __name__ == '__main__':
    if not exists('bscript'):
        print >>sys.stderr, '\x1b[01;91mno bscript file found\x1b[0m'
        sys.exit(5)

    with open('bscript') as f:
        exec_script(f.read())

