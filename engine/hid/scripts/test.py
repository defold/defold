#!/usr/bin/env python

import os, sys, run

def Usage():
    print("Usage:")
    print("  python %s <platform>" % os.path.basename(__file__))
    sys.exit(1)

def run_tests(builddir, platform):
    result = run.command(f'{builddir}/test_hid')
    print(result)

if __name__ == '__main__':
    platform = None
    if len(sys.argv) >= 2:
        platform = sys.argv[1]

    if platform is None:
        Usage()

    builddir = './build/bin/%s' % platform
    run_tests(builddir, platform)
