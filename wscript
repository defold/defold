#! /usr/bin/env python

VERSION = '0.1'
APPNAME = 'graphics'

srcdir = '.'
blddir = 'build'

import os, sys

import waf_ddf

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
        conf.env['CXXFLAGS']='-g -D__STDC_LIMIT_MACROS -Wall'
    else:
        conf.env['CXXFLAGS']=['/Z7', '/MT', '/D__STDC_LIMIT_MACROS', '/DGL_GLEXT_PROTOTYPES']
        conf.env.append_value('CPPPATH', "../include/win32")

    if platform == "linux":
        conf.env.append_value('CXXFLAGS', '-DGL_GLEXT_PROTOTYPES')

    if platform == "darwin":
        conf.env['CXXFLAGS']=['-framework GLUT -framework GL']

    if os.getenv('PYTHONPATH'):
        if sys.platform == "win32":
            ddf_search_lst = os.getenv('PYTHONPATH').split(';')
        else:
            ddf_search_lst = os.getenv('PYTHONPATH').split(':')

    for d in ddf_search_lst:
        if os.path.isfile(os.path.join(d, "ddf.py")):
            conf.env['DDF_PY'] = os.path.abspath(os.path.join(d, "ddf.py"))

    if not conf.env['DDF_PY']:
        conf.fatal("ddf.py not found in: " + str(ddf_search_lst))

    dynamo_home = os.getenv('DYNAMO_HOME')
    if not dynamo_home:
        conf.fatal("DYNAMO_HOME not set")
    dynamo_ext = os.path.join(dynamo_home, "ext")

    conf.env.append_value('CPPPATH', "../src")
    conf.env.append_value('CPPPATH', os.path.join(dynamo_ext, "include"))
    conf.env.append_value('LIBPATH', os.path.join(dynamo_ext, "lib", platform))
    conf.env.append_value('CPPPATH', os.path.join(dynamo_home, "include" ))
    conf.env.append_value('CPPPATH', os.path.join(dynamo_home, "include", platform))

    conf.env['LIB_GTEST'] = 'gtest'

def build(bld):
    bld.add_subdirs('src')
    bld.install_files('${PREFIX}/include/win32', 'include/win32/*.h')


