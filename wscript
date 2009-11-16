#! /usr/bin/env python

VERSION='0.1'
APPNAME='gamesys'

srcdir = '.'
blddir = 'build'

import sys
import waf_dynamo, waf_ddf

def init():
    pass

def set_options(opt):
    opt.sub_options('src')
    opt.tool_options('compiler_cxx')
    opt.tool_options('python')

def configure(conf):
    conf.check_tool('compiler_cxx')
    conf.check_tool('java')

    conf.check_tool('python')
    conf.check_python_version((2,5))
    conf.check_python_headers()

    waf_dynamo.configure(conf)
    waf_ddf.configure(conf)

    conf.sub_config('src')

    conf.env.append_value('CPPPATH', "default/src")
    conf.env['LIB_GTEST'] = 'gtest'
    conf.env['STATICLIB_DLIB'] = 'dlib'
    conf.env['STATICLIB_DDF'] = 'ddf'

def build(bld):
    bld.add_subdirs('src')
    
import Build, Options
import os, subprocess
def shutdown():
    waf_dynamo.run_gtests(valgrind = True)
