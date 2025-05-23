#!/usr/bin/env python
# Copyright 2020-2025 The Defold Foundation
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



import re
import logging
import sys
import io
from optparse import OptionParser
from markdown import Markdown
from markdown import Extension
from markdown.util import etree, AtomicString
from markdown.inlinepatterns import Pattern
from pprint import pprint
from google.protobuf.descriptor import FieldDescriptor
from google.protobuf.message import Message

import json
import yaml
import script_doc_ddf_pb2


# JG: reference pattern is 15 in blockprocessors.py, so I'm guessing it's supposed to be lower?
REF_PATTERN_PRIORITY_VALUE = 14

#
#   This extension allows the use of [ref:go.animate] or [ref:animate] reference tags in source
#
class RefExtensionException(Exception):
    pass

class RefExtension(Extension):
    def extendMarkdown(self, md, md_globals):
        pattern = r'\[(?P<prefix>ref):(?P<type>.+?)\]'
        tp = RefPattern(pattern)
        tp.md = md
        tp.ext = self

        # Add inline pattern before "reference" pattern
        md.inlinePatterns.register(tp, "ref", REF_PATTERN_PRIORITY_VALUE)

class RefPattern(Pattern):
    def getCompiledRegExp(self):
        return re.compile("^(.*?)%s(.*)$" % self.pattern, re.DOTALL | re.UNICODE | re.IGNORECASE)

    def handleMatch(self, m):
        ref = m.group(3)
        s = ref.split('.')
        if len(s) == 2:
            refurl = ('/ref/%s#%s') % (s[0], ref)
        else:
            refurl = ('#%s') % (s[0])
        el = etree.Element('a')
        el.text = AtomicString(ref)
        el.set('href', refurl)
        return el

#
#   This extension allows the use of [icon:icon_type] tags in source
#
class IconExtensionException(Exception):
    pass

class IconExtension(Extension):
    def extendMarkdown(self, md, md_globals):
        pattern = r'\[(?P<prefix>icon):(?P<type>[\w| ]+)\]'
        tp = IconPattern(pattern)
        tp.md = md
        tp.ext = self
        # Add inline pattern before "reference" pattern
        md.inlinePatterns.register(tp, "icon", REF_PATTERN_PRIORITY_VALUE)

class IconPattern(Pattern):
    def getCompiledRegExp(self):
        return re.compile("^(.*?)%s(.*)$" % self.pattern, re.DOTALL | re.UNICODE | re.IGNORECASE)

    def handleMatch(self, m):
        el = etree.Element('span')
        el.set('class', 'icon-' + m.group(3).lower())
        return el

#
#   This extension allows the use of [type:some_type] tags in source
#
class TypeExtensionException(Exception):
    pass

class TypeExtension(Extension):
    def extendMarkdown(self, md, md_globals):
        pattern = r'\[(?P<prefix>type):(?P<type>.+?)\]'
        tp = TypePattern(pattern)
        tp.md = md
        tp.ext = self
        # Add inline pattern before "reference" pattern
        md.inlinePatterns.register(tp, "type", REF_PATTERN_PRIORITY_VALUE)

class TypePattern(Pattern):
    def getCompiledRegExp(self):
        return re.compile("^(.*?)%s(.*)$" % self.pattern, re.DOTALL | re.UNICODE | re.IGNORECASE)

    def handleMatch(self, m):
        el = etree.Element('span')
        el.set('class', 'type')
        types = m.group(3)
         # Make sure types are shown as type1 | type2
        types = re.sub(' or ', ' | ', types)
        types = re.sub(r'(?<=\w)[|](?=\w)', ' | ', types)
        el.text = AtomicString(types)
        return el

#
#   This extension allows the use of [CLASS:TEXT] tags in source
#   that will be expanded to <span class="CLASS">TEXT</span>.
#
class ClassExtensionException(Exception):
    pass

class ClassExtension(Extension):
    def extendMarkdown(self, md, md_globals):
        pattern = r'\[(?P<prefix>[a-z0-9-_]+):(?P<text>.+?)\]'
        cp = ClassPattern(pattern)
        cp.md = md
        cp.ext = self
        # Add inline pattern before "reference" pattern
        md.inlinePatterns.register(cp, "class", REF_PATTERN_PRIORITY_VALUE)

class ClassPattern(Pattern):
    def getCompiledRegExp(self):
        return re.compile("^(.*?)%s(.*)$" % self.pattern, re.DOTALL | re.UNICODE | re.IGNORECASE)

    def handleMatch(self, m):
        el = etree.Element('span')
        el.set('class', m.group(2).lower())
        el.text = AtomicString(m.group(3))
        return el


#
#   Instance a markdown converter with some useful extensions
#
md = Markdown(extensions=['markdown.extensions.fenced_code','markdown.extensions.def_list',
    'markdown.extensions.codehilite','markdown.extensions.tables',
    TypeExtension(), IconExtension(), RefExtension(), ClassExtension()])

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

# Converts "<p>text</p>" to "text"
def _strip_paragraph(text):
    return text.replace('<p>', '').replace('</p>', '')

def _markdownify(t):
    md.reset()
    t = md.convert(t)
    return _strip_paragraph(t)


def tags_to_proto_type(tags):
    # we sometimes annotate with "@type typedef" or "@type class" instead
    # of "@typedef" and "@class"
    if 'type' in tags:
        value = tags["type"]
        if tags.get(value) is None:
            tags[value] = ""

    if   'variable' in tags: return script_doc_ddf_pb2.VARIABLE
    elif 'message' in tags: return script_doc_ddf_pb2.MESSAGE
    elif 'property' in tags: return script_doc_ddf_pb2.PROPERTY
    elif 'struct' in tags: return script_doc_ddf_pb2.STRUCT
    elif 'class' in tags: return script_doc_ddf_pb2.CLASS
    elif 'macro' in tags: return script_doc_ddf_pb2.MACRO
    elif 'enum' in tags: return script_doc_ddf_pb2.ENUM
    elif 'typedef' in tags: return script_doc_ddf_pb2.TYPEDEF
    elif 'constant' in tags: return script_doc_ddf_pb2.CONSTANT
    return script_doc_ddf_pb2.FUNCTION


def proto_to_type(p):
    if p == script_doc_ddf_pb2.VARIABLE: return "variable"
    elif p == script_doc_ddf_pb2.MESSAGE: return 'message'
    elif p == script_doc_ddf_pb2.PROPERTY: return 'property'
    elif p == script_doc_ddf_pb2.STRUCT: return 'struct'
    elif p == script_doc_ddf_pb2.CLASS: return 'class'
    elif p == script_doc_ddf_pb2.MACRO: return 'macro'
    elif p == script_doc_ddf_pb2.ENUM: return 'enum'
    elif p == script_doc_ddf_pb2.TYPEDEF: return 'typedef'
    elif p == script_doc_ddf_pb2.CONSTANT: return 'constant'
    return "function"

def _parse_description(text):
    desc_start = min(len(text), text.find('\n'))
    brief = text[0:desc_start]
    desc_end = min(len(text), text.find('\n@'))
    description = text[desc_start:desc_end].strip()
    if not brief and description:
        brief = description.split('.\n')[0]
        if len(brief) > 50: # trial and error of what fits into a single line
            brief = brief[:50] + '...'
    elif not description and brief:
        description = brief
    return description, brief

def _parse_path(path):
    file = ""
    if path:
        index = path.find('dmsdk')
        if index != -1:
            file = path[index:]
    return path, file

def _create_doc_info(tags):
    info = script_doc_ddf_pb2.Info()
    info.path, info.file = _parse_path(tags["path"])
    if tags["path"] == "":
        logging.warning('Missing tag @path for @name %s' % tags["name"])
    if 'namespace' in tags:
        info.namespace = tags["namespace"]
    return info

def _create_doc_element(tags):
    element = script_doc_ddf_pb2.Element()
    element.type = tags_to_proto_type(tags)

    for value in tags["return"]:
        """ Some of the possible variations:
        @return name
        @return [type:type]
        @return [type:type] doc
        @return name [type:type] doc
        """
        tmp = value.split(' ', 1)
        if len(tmp) < 2:
            tmp = [tmp[0], '']
        if '[type:' in tmp[0]:
            tmp = ['', value]
        ret = element.returnvalues.add()
        ret.name = tmp[0]
        types, ret.doc = extract_type_from_docstr(tmp[1])
        if isinstance(types, str):
            types = [types]
        ret.types.extend(types)

    for value in tags["param"]:
        tmp = value.split(' ', 1)
        if len(tmp) < 2:
            tmp = [tmp[0], '']
        parameter = element.parameters.add()
        parameter.is_optional, parameter.name = is_optional(tmp[0])
        types, parameter.doc = extract_type_from_docstr(tmp[1])
        if isinstance(types, str):
            types = [types]
        parameter.types.extend(types)

    for value in tags["tparam"]:
        tmp = value.split(' ', 1)
        if len(tmp) < 2:
            tmp = [tmp[0], '']
        tparam = element.tparams.add()
        tparam.name = tmp[0]
        tparam.type, tparam.doc = extract_type_from_docstr(tmp[1])

    for value in tags["member"]:
        tmp = value.split(' ', 1)
        if len(tmp) < 2:
            tmp = [tmp[0], '']
        member = element.members.add()
        member.name = tmp[0]
        member.type, member.doc = extract_type_from_docstr(tmp[1])

    if 'examples' in tags:
        element.examples = "".join(tags["examples"])

    if 'replaces' in tags:
        element.replaces = tags["replaces"]

    return element

def _parse_comment(text):
    text = _strip_comment_stars(text)
    # The regexp means match all strings that:
    # * begins with line start, possible whitespace and an @
    # * followed by non-white-space (the tag)
    # * followed by possible spaces
    # * followed by every character that is not an @ or is an @ but not preceded by a new line (the value)
    lst = re.findall('^\s*@(\S+) *((?:[^@]|(?<!\n)@)*)', text, re.MULTILINE)
    tags = {
        "path": "",
        "language": "",
        "param": [],
        "tparam": [],
        "note": [],
        "return": [],
        "member": [],
        "examples": [],
    }
    for tag,value in lst:
        tag = tag.strip()
        value = value.strip()
        v = tags.get(tag)
        if type(v) is list:
            v.append(value)
        else:
            tags[tag] = value

    # @struct somename
    if "struct" in tags and "name" not in tags:
        logging.warning('Missing tag @name, using value %s from @struct' % tags["struct"])
        tags["name"] = tags["struct"]

    if "name" not in tags:
        logging.warning('Missing tag @name in "%s"' % text)
        return None

    element = None
    if "document" in tags:
        element = _create_doc_info(tags)
    else:
        element = _create_doc_element(tags)

    element.description, element.brief = _parse_description(text)
    element.name = tags["name"]
    element.language = tags["language"]
    element.notes.extend(tags["note"])

    return element

def extract_type_from_docstr(s):
    # try to extract the type information
    m = re.search(r'^\s*(?:\s*\[type:\s*([^\]]*)\])+\s*([\w\W]*)', s)
    if m and m.group(1):
        type_list = m.group(1).split("|")
        if len(type_list) == 1:
            type_list = type_list[0]
        if m.group(2):
            return type_list, m.group(2)
        return type_list, ""

    return "", s

def is_optional(str):
    m = re.search('^\[(.*)\]', str)
    if m and m.group(1):
        return True, m.group(1)

    return False, str


def parse_document(doc_str):
    doc = script_doc_ddf_pb2.Document()
    lst = re.findall('/\*#(.*?)\*/', doc_str, re.DOTALL)
    element_list = []
    doc_info = None
    for comment_str in lst:
        element = _parse_comment(comment_str)
        if type(element) is script_doc_ddf_pb2.Element:
            element_list.append(element)
        elif type(element) is script_doc_ddf_pb2.Info:
            doc_info = element

    if doc_info:
        doc.info.CopyFrom(doc_info)

    for i, e in enumerate(element_list):
        doc.elements.add().MergeFrom(e)

    if doc.info.language == "":
        if doc.info.namespace.startswith("dm"):
            doc.info.language = "C++"
        else:
            doc.info.language = "Lua"

    return doc

def message_to_dict(message):
    ret = {}
    for field in message.DESCRIPTOR.fields:
        value = getattr(message, field.name)
        if field.label == FieldDescriptor.LABEL_REPEATED:
            lst = []
            for element in value:
                if isinstance(element, Message):
                    lst.append(message_to_dict(element))
                else:
                    lst.append(str(element))
            ret[field.name] = lst
        elif field.type == FieldDescriptor.TYPE_MESSAGE:
            ret[field.name] = message_to_dict(value)
        elif field.type == FieldDescriptor.TYPE_ENUM:
            ret[field.name] = field.enum_type.values_by_number[value].name
        else:
            ret[field.name] = str(value)
    return ret

def message_to_json_dict(message):
    d = message_to_dict(message)
    for e in d["elements"]:
        e["description"] = _markdownify(e["description"])
        e["brief"] = _markdownify(e["brief"])
        e["examples"] = _markdownify(e["examples"])
        e["replaces"] = _markdownify(e["replaces"])
        for p in e["parameters"]:
            p["doc"] = _markdownify(p["doc"])
        for r in e["returnvalues"]:
            r["doc"] = _markdownify(r["doc"])
    return d

def to_list(p):
    if len(p) == 1: return p[0]
    l = []
    for v in p:
        l.append(v)
    return l

def message_to_yaml_dict(message):
    info = message.info
    elements = message.elements

    api = []

    module_name = info.namespace
    module = {
        'name': module_name,
        'type': "table",
        'desc': info.description,
        'members': []
    }

    for element in elements:
        # name
        elem_name = element.name
        part_of_ns = elem_name.startswith(module_name)
        if part_of_ns:
            elem_name = elem_name[len(module_name)+1:]

        # type
        elem_type = proto_to_type(element.type)
        if elem_type == "variable":
            elem_type = "number"

        # desc
        elem_desc = element.description
        if len(elem_desc) == 0:
            elem_desc = element.brief

        # notes
        elem_notes = to_list(element.notes)

        entry = {
            'name': elem_name,
            'type': elem_type,
            'desc': elem_desc,
            'notes': elem_notes
        }

        # type specific fields
        if elem_type == "function":

            # parameters for functions
            elem_params = []
            for param in element.parameters:
                func_param = {
                        'name': param.name,
                        'desc': param.doc,
                        'type': to_list(param.types)
                    }
                if param.is_optional:
                    func_param["optional"] = True

                elem_params.append(func_param)

            entry["parameters"] = elem_params

            # function returns
            elem_returns = []
            for ret in element.returnvalues:
                elem_returns.append({
                        'desc': ret.doc,
                        'type': to_list(ret.types)
                    })
            if len(elem_returns) > 0:
                entry["returns"] = elem_returns

        if part_of_ns:
            module["members"].append(entry)
        else:
            api.append(entry)

    api.append(module)

    return api

if __name__ == '__main__':
    usage = "usage: %prog [options] INFILE(s) OUTFILE"
    parser = OptionParser(usage = usage)
    parser.add_option("-t", "--type", dest="type",
                      help="Supported formats: protobuf, json and script_api. default is protobuf", metavar="TYPE", default='protobuf')
    (options, args) = parser.parse_args()

    if len(args) < 2:
        parser.error('At least two arguments required')

    doc_str = ''
    for name in args[:-1]:
        with open(name, 'r') as f:
            doc_str += f.read()

    output_file = args[-1]
    f = open(output_file, "w")
    if options.type == 'protobuf':
        doc_msg = parse_document(doc_str)
        f.write(str(doc_msg))
    elif options.type == 'json':
        doc_msg = parse_document(doc_str)
        doc_dict = message_to_json_dict(doc_msg)
        json.dump(doc_dict, f, indent = 2)
    elif options.type == 'script_api':
        doc_msg = parse_document(doc_str)
        doc_dict = message_to_yaml_dict(doc_msg)
        yaml.dump(doc_dict, f, default_flow_style = False)
    else:
        print ('Unknown type: %s' % options.type)
        sys.exit(5)
    f.close()


