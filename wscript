#! /usr/bin/env python

DDF_MAJOR_VERSION=1
DDF_MINOR_VERSION=0

VERSION='%d.%d' % (DDF_MAJOR_VERSION, DDF_MINOR_VERSION)
APPNAME='ddf'

srcdir = '.'
blddir = 'build'

import os, sys, re
sys.path = ["src"] + sys.path
import waf_ddf, waf_dynamo

def init():
    pass

def set_options(opt):
    opt.sub_options('src')
    opt.tool_options('compiler_cxx')

def configure(conf):
    # Replace version number in python file.
    ddfc_py_str = ddfc_py_str_orig = open('src/ddfc.py', 'rb').read()
    ddfc_py_str = re.sub('DDF_MAJOR_VERSION=(\d*)', 'DDF_MAJOR_VERSION=%d' % DDF_MAJOR_VERSION, ddfc_py_str)
    ddfc_py_str = re.sub('DDF_MINOR_VERSION=(\d*)', 'DDF_MINOR_VERSION=%d' % DDF_MINOR_VERSION, ddfc_py_str)
    if ddfc_py_str != ddfc_py_str_orig:
        open('src/ddfc.py', 'wb').write(ddfc_py_str)

    # Create config.h with version numbers
    conf.define('DDF_MAJOR_VERSION', DDF_MAJOR_VERSION)
    conf.define('DDF_MINOR_VERSION', DDF_MINOR_VERSION)
    conf.write_config_header('config.h')

    conf.check_tool('compiler_cxx')
    conf.check_tool('java')

    conf.sub_config('src')

    conf.find_file('ddfc.py', var='DDFC', path_list = [os.path.abspath('src')], mandatory = True)

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
        conf.env['CXXFLAGS']=['-g', '-D', '__STDC_LIMIT_MACROS']
    else:
        conf.env['CXXFLAGS']=['/Z7', '/MT', '/D__STDC_LIMIT_MACROS']
        conf.env.append_value('CPPPATH', "../src/win32")

    conf.env['LIB_PROTOBUF'] = 'protobuf'
    conf.env['LIB_GTEST'] = 'gtest'

def build(bld):
    bld.add_subdirs('src')

def shutdown():
    import Options, Build

    waf_dynamo.run_gtests(valgrind = True)

    if Options.commands['build']:
        dynamo_home = Build.bld.get_env()['DYNAMO_HOME']
        cp = os.pathsep.join([dynamo_home + '/ext/share/java/protobuf-java-2.0.3.jar',
                              dynamo_home + '/ext/share/java/junit-4.6.jar',
                              'build/default/src/java_test',
                              'build/default/src/test/generated',
                              'build/default/src/java'])
        cmd = """
%s -cp %s org.junit.runner.JUnitCore com.dynamo.format.test.FormatLoaderTest
""" % (Build.bld.get_env()['JAVA'][0], cp)
        ret = os.system(cmd)
        if ret != 0:
            sys.exit(ret)

