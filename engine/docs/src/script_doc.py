#!/usr/bin/env python

import re
import logging
import sys
import StringIO
from optparse import OptionParser
from markdown import Markdown
from markdown import Extension
from markdown.util import etree, AtomicString
from markdown.inlinepatterns import Pattern
from pprint import pprint
import yaml

import script_doc_ddf_pb2

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
        md.inlinePatterns.add("ref", tp, "<reference")

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
        md.inlinePatterns.add("icon", tp, "<reference")

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
        md.inlinePatterns.add("type", tp, "<reference")

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
        md.inlinePatterns.add("class", cp, "<reference")

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

def _parse_comment(str):
    str = _strip_comment_stars(str)
    # The regexp means match all strings that:
    # * begins with line start, possible whitespace and an @
    # * followed by non-white-space (the tag)
    # * followed by possible spaces
    # * followed by every character that is not an @ or is an @ but not preceded by a new line (the value)
    lst = re.findall('^\s*@(\S+) *((?:[^@]|(?<!\n)@)*)', str, re.MULTILINE)

    name_found = False
    document_comment = False
    element_type = script_doc_ddf_pb2.FUNCTION
    for (tag, value) in lst:
        tag = tag.strip()
        value = value.strip()
        if tag == 'name':
            name_found = True
        elif tag == 'variable':
            element_type = script_doc_ddf_pb2.VARIABLE
        elif tag == 'message':
            element_type = script_doc_ddf_pb2.MESSAGE
        elif tag == 'property':
            element_type = script_doc_ddf_pb2.PROPERTY
        elif tag == 'struct':
            element_type = script_doc_ddf_pb2.STRUCT
        elif tag == 'macro':
            element_type = script_doc_ddf_pb2.MACRO
        elif tag == 'enum':
            element_type = script_doc_ddf_pb2.ENUM
        elif tag == 'typedef':
            element_type = script_doc_ddf_pb2.TYPEDEF
        elif tag == 'document':
            document_comment = True

    if not name_found:
        logging.warn('Missing tag @name in "%s"' % str)
        return None

    desc_start = min(len(str), str.find('\n'))
    brief = str[0:desc_start]
    desc_end = min(len(str), str.find('\n@'))
    description = str[desc_start:desc_end].strip()

    element = None

    if document_comment:
        element = script_doc_ddf_pb2.Info()
        element.brief = md.convert(brief)
        element.description = md.convert(description)
    else:
        element = script_doc_ddf_pb2.Element()
        element.type = element_type
        element.brief = md.convert(brief)
        element.description = md.convert(description)

    namespace_found = False
    for (tag, value) in lst:
        value = value.strip()
        md.reset()
        if tag == 'name':
            element.name = value
        elif tag == 'return':
            tmp = value.split(' ', 1)
            if len(tmp) < 2:
                tmp = [tmp[0], '']
            ret = element.returnvalues.add()
            ret.name = tmp[0]
            ret.doc = md.convert(tmp[1])
        elif tag == 'param':
            tmp = value.split(' ', 1)
            if len(tmp) < 2:
                tmp = [tmp[0], '']
            param = element.parameters.add()
            param.name = tmp[0]
            param.doc = md.convert(tmp[1])
        elif tag == 'member':
            tmp = value.split(' ', 1)
            if len(tmp) < 2:
                tmp = [tmp[0], '']
            mem = element.members.add()
            mem.name = tmp[0]
            mem.doc = md.convert(tmp[1])
        elif tag == 'examples':
            element.examples = md.convert(value)
        elif tag == 'replaces':
            element.replaces = md.convert(value)
        elif tag == 'namespace' and document_comment:
            # only care for @namespace in @document comments.
            element.namespace = value
            namespace_found = True

    if document_comment and not namespace_found:
        logging.warn('Missing tag @namespace in "%s"' % str)
        return None

    return element

def extract_type_from_docstr(str):
    # try to extract the type information
    m = re.search('^\[type:(.*)\] (.*)', str)
    if m and m.group(1) and m.group(2):
        type_list = m.group(1).split("|")
        if len(type_list) == 1:
            type_list = type_list[0]
        return type_list, m.group(2)

    return "", str

def is_optional(str):
    m = re.search('^\[(.*)\]', str)
    if m and m.group(1):
        return True, m.group(1)

    return False, str

def _parse_comment_yaml(str):
    str = _strip_comment_stars(str)
    # The regexp means match all strings that:
    # * begins with line start, possible whitespace and an @
    # * followed by non-white-space (the tag)
    # * followed by possible spaces
    # * followed by every character that is not an @ or is an @ but not preceded by a new line (the value)
    lst = re.findall('^\s*@(\S+) *((?:[^@]|(?<!\n)@)*)', str, re.MULTILINE)

    name_found = False
    element_type = "function"
    for (tag, value) in lst:
        tag = tag.strip()
        value = value.strip()
        if tag == 'name':
            name_found = True
        elif tag == 'variable':
            element_type = "variable"
        elif tag == 'message':
            element_type = "message"
        elif tag == 'property':
            element_type = "property"
        elif tag == 'struct':
            element_type = "struct"
        elif tag == 'macro':
            element_type = "macro"
        elif tag == 'enum':
            element_type = "enum"
        elif tag == 'typedef':
            element_type = "typedef"
        elif tag == 'document':
            element_type = "document"

    if not name_found:
        logging.warn('Missing tag @name in "%s"' % str)
        return None

    desc_start = min(len(str), str.find('\n'))
    brief = str[0:desc_start]
    desc_end = min(len(str), str.find('\n@'))
    description = str[desc_start:desc_end].strip()

    element = {}
    element["type"] = element_type
    element["brief"] = brief
    element["description"] = description
    element["returns"] = []
    element["members"] = []
    element["params"] = []

    namespace_found = False
    for (tag, value) in lst:
        value = value.strip()
        if tag == 'name':
            element["name"] = value

        elif tag == 'return':
            tmp = value.split(' ', 1)
            if len(tmp) < 2:
                tmp = [tmp[0], '']
            ret = {}
            ret["name"] = tmp[0]
            ret["type"], ret["doc"] = extract_type_from_docstr(tmp[1])
            element["returns"].append(ret)

        elif tag == 'param':
            tmp = value.split(' ', 1)
            if len(tmp) < 2:
                tmp = [tmp[0], '']
            param = {}
            param["optional"], param["name"] = is_optional(tmp[0])
            # param["name"] = tmp[0]
            param["type"], param["doc"] = extract_type_from_docstr(tmp[1])
            element["params"].append(param)

        elif tag == 'member':
            tmp = value.split(' ', 1)
            if len(tmp) < 2:
                tmp = [tmp[0], '']
            mem = {}
            mem["name"] = tmp[0]
            mem["type"], mem["doc"] = extract_type_from_docstr(tmp[1])
            element["members"].append(mem)

        elif tag == 'examples':
            element["examples"] = value

        elif tag == 'replaces':
            element["replaces"] = value

        elif tag == 'namespace' and element_type == 'document':
            # only care for @namespace in @document comments.
            element["namespace"] = value
            namespace_found = True

    if element_type == 'document' and not namespace_found:
        logging.warn('Missing tag @namespace in "%s"' % str)
        return None

    return element

def parse_document_yaml(doc_str):
    lst = re.findall('/\*#(.*?)\*/', doc_str, re.DOTALL)
    info = {}
    element_list = []
    for comment_str in lst:
        element = _parse_comment_yaml(comment_str)
        if element == None:
            continue

        if element["type"] == 'document':
            info = element
        else:
            element_list.append(element)

    return info, element_list


def parse_document(doc_str):
    doc = script_doc_ddf_pb2.Document()
    lst = re.findall('/\*#(.*?)\*/', doc_str, re.DOTALL)
    element_list = []
    doc_info = None
    for comment_str in lst:
        element = _parse_comment(comment_str)
        if type(element) is script_doc_ddf_pb2.Element:
            element_list.append(element)
        if type(element) is script_doc_ddf_pb2.Info:
            doc_info = element

    if doc_info:
        doc.info.CopyFrom(doc_info)

    element_list.sort(cmp = lambda x,y: cmp(x.name, y.name))
    for i, e in enumerate(element_list):
        doc.elements.add().MergeFrom(e)

    return doc

from google.protobuf.descriptor import FieldDescriptor
from google.protobuf.message import Message
import json

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

def doc_to_ydict(info, elements):
    api = []

    module_name = info["namespace"]
    module = {
        'name': module_name,
        'type': "table",
        'desc': info["description"],
        'members': []
    }

    for element in elements:
        # name
        elem_name = element["name"]
        part_of_ns = elem_name.startswith(module_name)
        if part_of_ns:
            elem_name = elem_name[len(module_name)+1:]

        # type
        elem_type = element["type"]
        if elem_type == "variable":
            elem_type = "number"

        # desc
        elem_desc = element["description"]
        if len(elem_desc) == 0:
            elem_desc = element["brief"]

        entry = {
            'name': elem_name,
            'type': elem_type,
            'desc': elem_desc
        }

        # type specific fields
        if elem_type == "function":

            # parameters for functions
            elem_params = []
            for param in element["params"]:
                func_param = {
                        'name': param["name"],
                        'desc': param["doc"],
                        'type': param["type"]
                    }
                if param["optional"]:
                    func_param["optional"] = True

                elem_params.append(func_param)

            entry["parameters"] = elem_params

            # function returns
            elem_returns = []
            for ret in element["returns"]:
                elem_returns.append({
                        'desc': ret["doc"],
                        'type': ret["type"]
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
        f = open(name, 'rb')
        doc_str += f.read()
        f.close()

    doc = parse_document(doc_str)
    if doc:
        output_file = args[-1]
        f = open(output_file, "wb")
        if options.type == 'protobuf':
            f.write(doc.SerializeToString())
        elif options.type == 'json':
            doc_dict = message_to_dict(doc)
            json.dump(doc_dict, f, indent = 2)
        elif options.type == 'script_api':
            info, elements = parse_document_yaml(doc_str)
            doc_dict = doc_to_ydict(info, elements)
            yaml.dump(doc_dict, f, default_flow_style = False)
            # print(yaml.dump(doc_dict, default_flow_style = False))
        else:
            print 'Unknown type: %s' % options.type
            sys.exit(5)
        f.close()


