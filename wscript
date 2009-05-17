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
    waf_dynamo.configure(conf)
    waf_ddf.configure(conf)
    
    conf.check_tool('compiler_cxx')
    conf.check_tool('python')
    conf.check_python_version((2,5))
    conf.check_python_headers()

    conf.sub_config('src')

    conf.env.append_value('CPPPATH', "default/src")
    conf.env['LIB_GTEST'] = 'gtest'
    conf.env['LIB_DLIB'] = 'dlib'
    conf.env['LIB_DDF'] = 'ddf'

def build(bld):
    bld.add_subdirs('src')
    
import Build, Options
import os, subprocess
def shutdown():
    if not Options.commands['build']:
        return

    for t in  Build.bld.all_task_gen:
        if hasattr(t, 'uselib') and str(t.uselib).find("GTEST") != -1:
            output = t.path
            filename = os.path.join(output.abspath(t.env), t.target)
            proc = subprocess.Popen(filename)
            ret = proc.wait()
            if ret != 0:
                print("test failed %s" %(t.target) )
                sys.exit(ret)
