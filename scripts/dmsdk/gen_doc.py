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

import os, re
from collections import OrderedDict

# This code is from waf_dynamo.py
# Perhaps we can make it so that waf_dynamo uses this file instead?
def _strip_comment_stars(str):
    lines = str.split('\n')
    ret = []
    for line in lines:
        line = line.strip()
        if line.startswith('*'):
            line = line[1:]
            if line.startswith(' '):
                line = line[1:]
        ret.append(line)
    return '\n'.join(ret)

def _convert_type(d):
    types = ['typedef', 'function', 'struct', 'macro', 'constant', 'enum']
    # def _do_convert_type(s, v):
    #     ('type', s) if s in types else v

    # return [_do_convert_type(v[0], v) for v in infos]

    t = None
    for k, v in d.items():
        if k in types:
            t = k
            break
    if t is not None:
        d['type'] = t
        del d[t]
    return d

def _parse_description(s):
    """ Parse out the first row of the description (the brief)
    and also the description itself.
    """
    try:
        index = s.find('@')
        s = s[:index]
    finally:
        pass

    s = s.strip()
    while s.startswith('#'): # due to our doxygen format
        s = s[1:]

    lines = s.splitlines()

    brief = None
    if len(lines) > 0:
        brief = lines[0].strip()
        lines = lines[1:]

    return brief, '\n'.join(lines).strip()

def _parse_type_text(param):
    lst = re.findall(r'^([a-zA-Z0-0_]*)\s*\[\s*type:\s*(.*)\]\s*(.*)', param, re.DOTALL)
    if not lst:
        d = {}
        d['desc'] = param
        return  d

    lst = lst[0]
    d = {}
    d['name'] = lst[0]
    d['type'] = lst[1]
    d['desc'] = lst[2]
    return  d


def _parse_comment(source):
    s = _strip_comment_stars(source)
    # The regexp means match all strings that:
    # * begins with line start, possible whitespace and an @
    # * followed by non-white-space (the tag)
    # * followed by possible spaces
    # * followed by every character that is not an @ or is an @ but not preceded by a new line (the value)
    lst = re.findall('^\s*@(\S+) *((?:[^@]|(?<!\n)@)*)', s, re.MULTILINE)

    # Parse the descriptions
    brief, desc = _parse_description(s)

    info = OrderedDict()
    if brief:
        info['brief'] = brief
    if not desc:
        desc = brief
    info['desc'] = desc

    lists = {
        'param': 'params',
        'examples': 'examples',
        'note': 'notes',
        'member': 'members',
    }

    for (tag, value) in lst:
        tag = tag.strip()
        value = value.strip()

        if tag in lists.keys():
            listkey = lists[tag]
            if not listkey in info:
                info[listkey] = []

            if tag in ['param', 'member']:
                value = _parse_type_text(value)
                info[listkey].append( value )
            else:
                info[listkey].append( (tag, value) )

        else:
            if tag == 'return':
                value = _parse_type_text(value)

            info[tag] = value

    return info

def _parse_elements(source):
    lst = re.findall('/(\*#.*?)\*/', source, re.DOTALL)
    paragraphs = [] # Each separate /*# ... #/
    doc_info = {}
    for comment_str in lst:
        info = _parse_comment(comment_str)
        info = _convert_type(info)

        is_doc = info.get('document', None) is not None

        if is_doc:
            doc_info = info

            name = info.get('name', None)
            if is_doc and not name:
                bld.fatal("No api @name in @document:")
        else:
            #paragraphs.append('/' + comment_str + '*/')
            paragraphs.append(info)

    return doc_info, paragraphs

def parse_docs(path):
    ret = OrderedDict()
    with open(path, encoding='utf8') as in_f:
        source = in_f.read()

    info, parsed_elements = _parse_elements(source)
    if not info:
        print("No @document found in " + path)

    if not parsed_elements or len(parsed_elements) == 0:
        print("No documentation found in " + path)

    # import pprint
    # pprint.pp(info)
    # pprint.pp(parsed_elements)

    d = OrderedDict(info)
    d['items'] = parsed_elements
    return d
