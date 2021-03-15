#!/usr/bin/env python

""" Since github actions are quite difficult to use when writing scriptable things.
We rely on this script being able to output the unaltered text.
"""

import sys

def get_engine_channel(args):
    branch=args[0]
    t=branch.split('-')
    # if it only has one token
    t.append('dev')
    t.append('dev')
    branch_to_channel={'dev':'alpha','beta':'beta','master':'stable'}
    channel=branch_to_channel.get(t[2],'alpha')
    return channel

if __name__ == '__main__':
    command = sys.argv[1]
    if command == 'get_engine_channel':
        print get_engine_channel(sys.argv[2:])
