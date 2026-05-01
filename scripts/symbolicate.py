# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

import re, sys, os, subprocess

# Script to "symbolicate" an iOS crash log.
# Not particularly pretty script but it works...

def run_cmd(cmd):
    p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)
    return p.stdout.read()

def parse_stacktrace(cd, slide, load_addr):
    lst = []
    for x in re.findall(r'^Thread \d+( Crashed)?:(.*?)^[ ]*$', cd, re.MULTILINE | re.DOTALL):
        x = x[1]
        tmp = [ y.strip() for y in x.split('\n') if y.strip() != '' ]
        st = []
        for y in tmp:
            lst2 = filter(len, re.split(r'[ \t]', y))
            stack = int(lst2[2].strip(), 16)
            address = slide + stack - load_addr
            st.append((lst2[1], address))
        lst.append(st)
    return lst

def get_address_map(stack_trace):
    addresses = reduce(lambda x,y: x + y,  [ [ y[1] for y in x ] for x in stack_trace ], [])
    out = run_cmd('atos -arch arm64 -o "%s" %s' % (sys.argv[2], ' '.join(['0x%x' % a for a in addresses])))[1:-2]
    lst = [ s.strip().replace(',', '') for s in  out.split('\n') ]
    address_map = {}
    for i,s in enumerate(lst):
        address_map[addresses[i]] = s
    return address_map

def symbolicate(cd):
    id = re.search(r'^Identifier:[ ]*(.*?)$', cd, re.MULTILINE | re.DOTALL).groups()[0]
    load_addr = int(re.search(r'^Binary Images:.*?0x(.*?)[ ]', cd, re.MULTILINE | re.DOTALL).groups()[0], 16)
    uuid = re.search(r'^Binary Images:.*?<(.*?)>[ ]', cd, re.MULTILINE | re.DOTALL).groups()[0].lower()
    load_cmds = run_cmd('otool -l "%s"' % sys.argv[2])
    exe_uuid = re.search(r'^\s*?uuid\s*?(.*?)$', load_cmds, re.MULTILINE | re.DOTALL).groups()[0].strip().replace('-', '').lower()
    slide = int(re.search(r'segname __TEXT.*?vmaddr (.*?)$', load_cmds, re.MULTILINE | re.DOTALL).groups()[0].lower().strip(), 16)

    if uuid != exe_uuid:
        print 'FATAL: uuid mismatch. %s != %s' % (uuid, exe_uuid)
        sys.exit(5)

    stack_trace = parse_stacktrace(cd, slide, load_addr)
    address_map = get_address_map(stack_trace)

    for i, t in enumerate(stack_trace):
        print("thread %d:" % (i))
        for j, s in enumerate(t):
            address = s[1]
            print '%2s:    %30s  %s (0x%x)' % (j, s[0], str(address_map[address]), address)

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print 'usage: symbolicate <crashdump> <executable>'
        print ''
        sys.exit(5)
    cd = open(sys.argv[1], 'rb').read()
    symbolicate(cd)
