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
    opt.tool_options('compiler_cc')
    opt.tool_options('compiler_cxx')
    opt.tool_options('waf_dynamo')

def configure(conf):
    conf.check_tool('compiler_cc')
    conf.check_tool('compiler_cxx')
    conf.check_tool('java')
    conf.check_tool('waf_dynamo')

    waf_ddf.configure(conf)

    conf.sub_config('src')

    conf.env.append_value('CPPPATH', "default/src")
    conf.env['LIB_GTEST'] = 'gtest'
    conf.env['STATICLIB_DLIB'] = 'dlib'
    conf.env['STATICLIB_DDF'] = 'ddf'

    platform = conf.env['PLATFORM']

    if platform == "linux":
        conf.env['LIB_PLATFORM_SOCKET'] = ''
    elif 'darwin' in platform:
        conf.env['LIB_PLATFORM_SOCKET'] = ''
    else:
        conf.env['LIB_PLATFORM_SOCKET'] = 'WS2_32'

def build(bld):
    bld.add_subdirs('src')

import Build, Options
import os, subprocess
def shutdown():
    if not Options.commands['build']:
        return

    # TODO: Fix support for win32
    from Logs import warn, error
    import urllib2, time, atexit

    if sys.platform != 'win32':
        os.system('scripts/start_http_server.sh')
        atexit.register(os.system, 'scripts/stop_http_server.sh')

        start = time.time()
        while True:
            if time.time() - start > 5:
                error('HTTP server failed to start within 5 seconds')
                sys.exit(1)
            try:
                urllib2.urlopen('http://localhost:6123')
                break
            except urllib2.URLError:
                print('Waiting for HTTP testserver to start...')
                sys.stdout.flush()
                time.sleep(0.5)
    else:
        warn('HTTP tests not supported on Win32 yet')

    waf_dynamo.run_gtests(valgrind = True)
