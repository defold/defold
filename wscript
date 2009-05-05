#! /usr/bin/env python

VERSION='0.1'
APPNAME='ddf'

srcdir = '.'
blddir = 'build'

import os, sys
sys.path = ["src"] + sys.path
import waf_ddf, waf_dynamo

def init():
    pass

def set_options(opt):
    opt.sub_options('src')
    opt.tool_options('compiler_cxx')

def configure(conf):
    conf.check_tool('compiler_cxx')
    conf.sub_config('src')

    conf.find_program('ddfc.py', var='DDFC', path_list = [os.path.abspath('src')], mandatory = True)

    waf_dynamo.configure(conf)

    if sys.platform == "darwin":
        platform = "darwin"
    elif sys.platform == "linux2":
        platform = "linux"
    elif sys.platform == "win32":
        platform = "win32"
    else:
        conf.fatal("Unable to determine platform")

    if platform == "linux" or platform == "darwin":
        conf.env['CXXFLAGS']='-g -D__STDC_LIMIT_MACROS'
    else:
        conf.env['CXXFLAGS']=['/Z7', '/MT', '/D__STDC_LIMIT_MACROS']
        conf.env.append_value('CPPPATH', "../src/win32")

    conf.env['LIB_PROTOBUF'] = 'protobuf'
    conf.env['LIB_GTEST'] = 'gtest'

def build(bld):
    bld.add_subdirs('src')

def shutdown():
    waf_dynamo.run_gtests()
