#! /usr/bin/env python

VERSION='0.1'
APPNAME='tools'

srcdir = '.'
blddir = 'build'

import sys
import waf_dynamo, waf_ddf

def init():
    pass

def set_options(opt):
    opt.tool_options('compiler_cc')
    opt.tool_options('compiler_cxx')
    opt.tool_options('waf_dynamo')

def configure(conf):
    conf.check_tool('compiler_cc')
    conf.check_tool('compiler_cxx')
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

    if platform == "darwin":
        conf.env.append_value('LINKFLAGS', ['-framework', 'Cocoa', '-framework', 'OpenGL', '-framework', 'AGL', '-framework', 'IOKit', '-framework', 'Carbon'])
    if platform == "linux":
        conf.env.append_value('LINKFLAGS', ['-lX11', '-lGL'])
    if platform == "win32":
        conf.env.append_value('LINKFLAGS', ['opengl32.lib', 'user32.lib'])

    conf.env.append_value('CPPPATH', "default/src")
    conf.env['LIB_GTEST'] = 'gtest'
    conf.env['STATICLIB_DLIB'] = 'dlib'
    conf.env['STATICLIB_DDF'] = 'ddf'
    conf.env['STATICLIB_HID'] = 'hid'
    conf.env['STATICLIB_INPUT'] = 'input'
    conf.env['STATICLIB_DMGLFW'] = 'dmglfw'

    platform = conf.env['PLATFORM']

    if platform == "linux":
        conf.env['LIB_PLATFORM_SOCKET'] = ''
    elif 'darwin' in platform:
        conf.env['LIB_PLATFORM_SOCKET'] = ''
    else:
        conf.env['LIB_PLATFORM_SOCKET'] = 'WS2_32'

    conf.env.append_unique('CCDEFINES', 'DLIB_LOG_DOMAIN="TOOLS"')
    conf.env.append_unique('CXXDEFINES', 'DLIB_LOG_DOMAIN="TOOLS"')

def build(bld):
    bld.add_subdirs('scripts')
    bld.add_subdirs('src')

def shutdown():
    waf_dynamo.run_gtests(valgrind = True)
