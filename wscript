#! /usr/bin/env python

VERSION = '0.1'
APPNAME = 'dlib'

srcdir = '.'
blddir = 'build'

import os, sys, waf_dynamo

def init():
    pass

def set_options(opt):
    opt.sub_options('src')
    opt.tool_options('compiler_cxx')

def configure(conf):
    conf.check_tool('compiler_cxx')
    conf.sub_config('src')

    if sys.platform == "darwin":
        platform = "darwin"
    elif sys.platform == "linux2":
        platform = "linux"
    elif sys.platform == "win32":
        platform = "win32"
    else:
        conf.fatal("Unable to determine platform")

    if platform == "linux" or platform == "darwin":
        conf.env['CXXFLAGS']=['-g', '-D__STDC_LIMIT_MACROS', '-Wall']
    else:
        conf.env['CXXFLAGS']=['/Z7', '/MT', '/D__STDC_LIMIT_MACROS']
        conf.env.append_value('CPPPATH', "../include/win32")

    dynamo_home = os.getenv('DYNAMO_HOME')
    if not dynamo_home:
        conf.fatal("DYNAMO_HOME not set")
    dynamo_ext = os.path.join(dynamo_home, "ext")

    conf.env.append_value('CPPPATH', os.path.join(dynamo_ext, "include"))
    conf.env.append_value('LIBPATH', os.path.join(dynamo_ext, "lib", platform))

    conf.env['LIB_GTEST'] = 'gtest'

    if sys.platform == "linux2":
        conf.env['LIB_THREAD'] = 'pthread'
    else:
        conf.env['LIB_THREAD'] = ''

def build(bld):
    bld.add_subdirs('src')
    bld.install_files('${PREFIX}/include/win32', 'include/win32/*.h')

    import Build
    import os
    cwd = os.getcwd()
    sources = set()
    for t in  Build.bld.all_task_gen:
        #print dir(t.path)
        p = t.path.abspath()
        if type(t.source) == str:
            sources.add(t.source)
        else:
            for s in t.source:
                s = os.path.join(p, s)
                s = s.replace(cwd, '')
                if s[0] == '/' or s[0] == '\\':
                    s = s[1:]
                sources.add(s)
    if Build.bld:
        tmp = ' '.join( [ '-I %s' % x for x in Build.bld.all_envs.items()[0][1]['CPPPATH'] ])
        for s in sources:
            print 'g++ %s -c %s' % (tmp, s)

def shutdown():
    waf_dynamo.run_gtests(valgrind = True)

