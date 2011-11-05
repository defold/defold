from __future__ import with_statement
import re
from os.path import join, normpath, exists, dirname
from bob import add_task, task, change_ext, execute_command, find_cmd, register

def config(prj):
    register(prj, '.c', c_lang_factory)
    find_cmd(prj, 'gcc', 'cc')

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

def c_lang_factory(p, input):
    options = [ '-I%s' % x for x in p['includes']]
    return add_task(p, task(function = c_lang_file,
                            name = 'c',
                            inputs = [input],
                            outputs = [change_ext(p, input, '.o')],
                            scanner = c_scanner,
                            options = options))
