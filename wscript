#! /usr/bin/env python

VERSION = '0.1'
APPNAME = 'render'


srcdir = '.'
blddir = 'build'

import os, sys
import Options

import waf_ddf, waf_dynamo

def init():
    pass

def set_options(opt):
    opt.sub_options('src')
    opt.tool_options('compiler_cxx')
    opt.tool_options('waf_dynamo')

def configure(conf):
    conf.check_tool('compiler_cxx')
    conf.check_tool('java')
    conf.check_tool('python')
    conf.check_python_version((2,5))
    conf.check_python_headers()

    conf.check_tool('waf_dynamo')
    waf_ddf.configure(conf)

    conf.sub_config('src')

    if sys.platform == "darwin":
        platform = "darwin"
    elif sys.platform == "linux2":
        platform = "linux"
    elif sys.platform == "win32":
        platform = "win32"
    else:
        conf.fatal("Unable to determine platform")

    if platform == "linux":
        conf.env.append_value('CXXFLAGS', '-DGL_GLEXT_PROTOTYPES')

    if os.getenv('PYTHONPATH'):
        if sys.platform == "win32":
            ddf_search_lst = os.getenv('PYTHONPATH').split(';')
        else:
            ddf_search_lst = os.getenv('PYTHONPATH').split(':')


    if platform == "darwin":
        conf.env.append_value('LINKFLAGS', ['-framework', 'Cocoa', '-framework', 'OpenGL', '-framework', 'OpenAL', '-framework', 'AGL', '-framework', 'IOKit', '-framework', 'Carbon'])
    if platform == "linux":
        conf.env.append_value('LINKFLAGS', ['-lglut', '-lXext', '-lX11', '-lXi', '-lGL', '-lGLU', '-lpthread'])
    if platform == "win32":
        conf.env.append_value('LINKFLAGS', ['opengl32.lib', 'user32.lib'])


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
    conf.env['STATICLIB_GRAPHICS_NULL'] = 'graphics_null'
    conf.env['STATICLIB_GRAPHICS'] = 'graphics'
    conf.env['STATICLIB_DLIB'] = 'dlib'
    conf.env['STATICLIB_RESOURCE'] = 'resource'
    conf.env['STATICLIB_DDF'] = 'ddf'
    conf.env['STATICLIB_HID'] = 'hid'
    conf.env['STATICLIB_DMGLFW'] = 'dmglfw'


    conf.env.append_unique('CCDEFINES', 'DLIB_LOG_DOMAIN="RENDER"')
    conf.env.append_unique('CXXDEFINES', 'DLIB_LOG_DOMAIN="RENDER"')

def build(bld):
    bld.add_subdirs('src')
    bld.install_files('${PREFIX}/include/win32', 'include/win32/*.h')

def shutdown():

    waf_dynamo.run_gtests(valgrind = True)


