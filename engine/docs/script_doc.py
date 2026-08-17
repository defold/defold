#!/usr/bin/env python
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



import re
import logging
import sys
import io
import codecs
import html

from optparse import OptionParser
from markdown import Markdown
from markdown import Extension
from markdown.util import etree, AtomicString
from markdown.inlinepatterns import Pattern
from pprint import pprint
from google.protobuf.descriptor import FieldDescriptor
from google.protobuf.message import Message

import pystache
import json
import yaml
import script_doc_ddf_pb2


# Version of the JSON interchange format consumed by defold.github.io.
# Keep this separate from the Defold release/version: alpha, beta and stable
# documentation archives can contain different format versions during rollout.
JSON_FORMAT_VERSION = 2


LUA_MTL = """
--[[
Generated using Defold build pipeline

./scripts/build.py build_docs

{{ info.description }}
--]]

---@meta
---@diagnostic disable: lowercase-global
---@diagnostic disable: missing-return
---@diagnostic disable: duplicate-doc-param
---@diagnostic disable: duplicate-set-field
---@diagnostic disable: args-after-dots

---@class defold_api.{{ info.namespace }}
{{ info.namespace }} = {}

{{#elements}}
{{#is_variable}}
---{{ description }}
{{ info.namespace }}.{{ name }} = nil
{{/is_variable}}
{{#is_constant}}
---{{ description }}
{{ name }} = nil
{{/is_constant}}

{{#is_function}}
---{{ description }}
{{#parameters}}
---@param {{name}}{{#is_optional}}?{{/is_optional}} {{ types_string }} {{doc}}
{{/parameters}}
{{#returnvalues}}
---@return {{ types_string }} {{ name }} {{doc}}
{{/returnvalues}}
function {{ name }}({{ params_string }}) end

{{/is_function}}
{{/elements}}

return {{ info.namespace }}
"""



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


_FENCED_CODE_LINE = re.compile(r'^[ ]{0,3}(?P<fence>`{3,}|~{3,})(?P<suffix>.*)$')


def _validate_fenced_code_blocks(text, context):
    """Return validation errors for malformed Markdown fenced code blocks."""
    errors = []
    opening_fence = None
    opening_line = None

    for line_number, line in enumerate(text.splitlines(), 1):
        match = _FENCED_CODE_LINE.match(line)
        if not match:
            continue

        fence = match.group("fence")
        suffix = match.group("suffix")
        if opening_fence is None:
            opening_fence = fence
            opening_line = line_number
            continue

        if fence[0] != opening_fence[0] or len(fence) < len(opening_fence):
            continue

        if suffix.strip():
            errors.append(
                "%s has text after a closing '%s' fence on documentation line %d" %
                (context, fence, line_number))
            continue

        opening_fence = None
        opening_line = None

    if opening_fence is not None:
        errors.append(
            "%s has an unclosed '%s' fence opened on documentation line %d" %
            (context, opening_fence, opening_line))

    return errors


def _validate_document_markdown(doc, file):
    source = file if file else "<string>"
    errors = []

    def validate(text, context):
        for error in _validate_fenced_code_blocks(text, context):
            errors.append("%s in file %s" % (error, source))

    validate(doc.info.description, "document description")
    for element in doc.elements:
        validate(element.description, "'%s' description" % element.name)
        validate(element.examples, "'%s' examples" % element.name)
        validate(element.replaces, "'%s' replacement documentation" % element.name)
        for note in element.notes:
            validate(note, "'%s' note" % element.name)
        for parameter in element.parameters:
            validate(parameter.doc, "'%s' parameter '%s'" % (element.name, parameter.name))
        for returnvalue in element.returnvalues:
            validate(returnvalue.doc, "'%s' return value '%s'" % (element.name, returnvalue.name))
        for member in element.members:
            validate(member.doc, "'%s' member '%s'" % (element.name, member.name))

    return errors


def _validate_structured_documentation(doc, file):
    """Reject code blocks that would be rendered inside API field tables."""
    if doc.info.language != "Lua":
        return []

    source = file if file else "<string>"
    errors = []

    def validate(text, context):
        if re.search(r'<pre(?:\s|>)', _markdownify(text)):
            errors.append(
                "%s contains a Markdown code block in file %s; "
                "move executable examples to @examples" % (context, source))

    for element in doc.elements:
        for parameter in element.parameters:
            validate(
                parameter.doc,
                "'%s' parameter '%s'" % (element.name, parameter.name))
        for returnvalue in element.returnvalues:
            validate(
                returnvalue.doc,
                "'%s' return value '%s'" % (element.name, returnvalue.name))
        for member in element.members:
            validate(
                member.doc,
                "'%s' member '%s'" % (element.name, member.name))

    return errors


_BOOLEAN_LITERAL_NAMES = frozenset(("true", "false"))


def _validate_document_names(doc, file):
    """Reject boolean literals used as names in generated API signatures."""
    source = file if file else "<string>"
    errors = []

    def validate_name(name, value_kind, element_name):
        if name in _BOOLEAN_LITERAL_NAMES:
            errors.append(
                "'%s' %s has invalid name '%s' in file %s" %
                (element_name, value_kind, name, source))

    def validate(values, value_kind, element_name):
        for value in values:
            validate_name(value.name, value_kind, element_name)

    for element in doc.elements:
        validate_name(element.name, "element", element.name)
        validate(element.parameters, "parameter", element.name)
        validate(element.returnvalues, "return value", element.name)
        validate(element.members, "member", element.name)
        validate(element.tparams, "template parameter", element.name)

    return errors


def _parse_tags(text):
    """Parse documentation tags without treating fenced code as tags."""
    parsed_tags = []
    current_tag = None
    current_value_lines = []
    opening_fence = None

    def finish_tag():
        if current_tag is not None:
            parsed_tags.append((current_tag, "\n".join(current_value_lines).strip()))

    for line in text.splitlines():
        tag_match = None
        if opening_fence is None:
            tag_match = re.match(r'^\s*@(\S+)(?:\s+(.*))?$', line)

        if tag_match:
            finish_tag()
            current_tag = tag_match.group(1)
            inline_value = tag_match.group(2)
            current_value_lines = [] if inline_value is None else [inline_value]
        elif current_tag is not None:
            current_value_lines.append(line)

        fence_match = _FENCED_CODE_LINE.match(line)
        if not fence_match:
            continue

        fence = fence_match.group("fence")
        suffix = fence_match.group("suffix")
        if opening_fence is None:
            opening_fence = fence
        elif (fence[0] == opening_fence[0]
              and len(fence) >= len(opening_fence)
              and not suffix.strip()):
            opening_fence = None

    finish_tag()
    return parsed_tags


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
    info.namespace = tags.get("namespace", "")
    return info

def _create_doc_element(tags):
    element = script_doc_ddf_pb2.Element()
    element.type = tags_to_proto_type(tags)

    if element.type == script_doc_ddf_pb2.CONSTANT and tags.get("constant"):
        value = element.parameters.add()
        value.name = "value"
        value.doc = ""
        types, _ = extract_type_from_docstr(tags["constant"])
        if isinstance(types, str):
            types = [types]
        value.types.extend(types)

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
        tparam.doc = tmp[1]
        tparam.type = ""

    for value in tags["member"]:
        tmp = value.split(' ', 1)
        if len(tmp) < 2:
            tmp = [tmp[0], '']
        member = element.members.add()
        member.name = tmp[0]
        member_types, member.doc = extract_type_from_docstr(tmp[1])
        member.type = '|'.join(member_types) if isinstance(member_types, list) else member_types

    if 'examples' in tags:
        element.examples = "\n\n".join(tags["examples"])

    if 'replaces' in tags:
        element.replaces = tags["replaces"]

    return element

def _parse_comment(text):
    text = _strip_comment_stars(text)
    lst = _parse_tags(text)
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

def split_type_union(type_expression):
    """Split a Lua type union without splitting nested type expressions."""
    parts = []
    start = 0
    stack = []
    pairs = {"]": "[", "}": "{", ")": "(", ">": "<"}
    for index, char in enumerate(type_expression):
        if char in "[{(<":
            stack.append(char)
        elif char in "]})>":
            if not stack or stack[-1] != pairs[char]:
                raise ValueError("Unbalanced Lua type expression: %s" % type_expression)
            stack.pop()
        elif char == "|" and not stack:
            parts.append(type_expression[start:index].strip())
            start = index + 1
    if stack:
        raise ValueError("Unbalanced Lua type expression: %s" % type_expression)
    parts.append(type_expression[start:].strip())
    if any(not part for part in parts):
        raise ValueError("Invalid Lua type union: %s" % type_expression)
    return parts


def _extract_balanced_type_tag(s):
    match = re.match(r"^\s*\[type:\s*", s)
    if not match:
        return None

    start = match.end()
    square_depth = 1
    brace_depth = 0
    paren_depth = 0
    angle_depth = 0
    for index in range(start, len(s)):
        char = s[index]
        if char == "[":
            square_depth += 1
        elif char == "]":
            square_depth -= 1
            if square_depth == 0 and brace_depth == 0 and paren_depth == 0 and angle_depth == 0:
                return s[start:index].strip(), s[index + 1:].lstrip()
            if square_depth < 0:
                break
        elif char == "{":
            brace_depth += 1
        elif char == "}":
            brace_depth -= 1
        elif char == "(":
            paren_depth += 1
        elif char == ")":
            paren_depth -= 1
        elif char == "<":
            angle_depth += 1
        elif char == ">":
            angle_depth -= 1

        if min(square_depth, brace_depth, paren_depth, angle_depth) < 0:
            break

    raise ValueError("Unbalanced [type:...] tag: %s" % s)


def extract_type_from_docstr(s):
    extracted = _extract_balanced_type_tag(s)
    if not extracted:
        return "", s

    type_expression, doc = extracted
    if not type_expression:
        return "", doc

    type_list = split_type_union(type_expression)
    return (type_list[0] if len(type_list) == 1 else type_list), doc

def is_optional(str):
    m = re.search(r'^\[(.*)\]', str)
    if m and m.group(1):
        return True, m.group(1)

    return False, str


LUA_TYPES = [
    "string", "number", "integer", "boolean", "table", "userdata", "nil", "function", "thread",
    "vector", "vector3", "vector4", "matrix4", "quaternion", "hash", "url", "node",
    "constant", "resource", "buffer", "any", "file",
    "b2World", "b2Body", "b2BodyType", "b2Shape", "b2Chain", "b2ContactEdge", "b2Transform", "b2MassData", "bufferstream",
    "resource_data", "buffer_data", "buffer_stream", "constant_buffer", "render_target", "render_predicate",
    "socket_client", "socket_master", "socket_unconnected" ]
CPP_TYPES = [
    "string", "float", "double", "long", "int", "bool", "char", "void",
    "int8_t", "uint8_t", "int16_t", "uint16_t", "int32_atomic_t", "int32_t", "uint32_t", "int64_t", "uint64_t",
    "uintptr_t",
    "size_t",
    "jobject", "JNIEnv",
    "lua_State",
    "dmhash_t", "dmArray", "dmAllocator" ]

def validate_lua_type(t, doc):
    # Lua types can refer to aliases, classes, enum families and messages from
    # other documents. They are therefore validated by lua_annotations.py only
    # after every Lua document and the official metadata have been loaded.
    return True

def validate_cpp_type(t, doc):
    t = t.replace("*", "").replace("&", "").replace("const ", "").replace("unsigned ", "")

    # ignore types with a namespace
    if "::" in t:
        return True

    # uint32_t:2 -> uint32_t
    if ":" in t:
        v = t.split(":")
        t = v[0]

    # dmArray<uint8_t> -> dmArray
    if "<" in t:
        v = t.split("<")
        t = v[0]

    # H = opaque handle
    if t.startswith("H"):
        return True
    # F = function typedef
    if t.startswith("F"):
        return True
    # function definition, see configfile.h as an example
    if t.startswith("function"):
        return True
    # varargs
    if t == "..." or t == "va_list":
        return True
    # extension macros and similar
    if t == "symbol":
        return True
    # standard C++ types
    if t in CPP_TYPES:
        return True
    # is type defined in the same document?
    for element in doc.elements:
        if element.name == t:
            return True

    # is type defined as a template parameter in the same document?
    for element in doc.elements:
        for tparam in element.tparams:
            if tparam.name == t:
                return True

    # print(doc)
    return False

def parse_document(doc_str, file=None):
    doc = script_doc_ddf_pb2.Document()
    lst = re.findall(r'/\*[\*#](.*?)\*/', doc_str, re.DOTALL)
    element_list = []
    doc_info = None
    markdown_errors = []
    source = file if file else "<string>"
    for comment_index, comment_str in enumerate(lst, 1):
        comment_text = _strip_comment_stars(comment_str)
        for error in _validate_fenced_code_blocks(
                comment_text, "documentation comment %d" % comment_index):
            markdown_errors.append("%s in file %s" % (error, source))

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

    markdown_errors.extend(_validate_document_markdown(doc, file))
    for err in markdown_errors:
        print("  ERROR", err)
    if markdown_errors:
        raise ValueError("Malformed Markdown fenced code block")

    structured_documentation_errors = _validate_structured_documentation(doc, file)
    for err in structured_documentation_errors:
        print("  ERROR", err)
    if structured_documentation_errors:
        raise ValueError("Code blocks are not allowed in structured documentation")

    value_name_errors = _validate_document_names(doc, file)
    for err in value_name_errors:
        print("  ERROR", err)
    if value_name_errors:
        raise ValueError("Invalid documentation value name")

    if doc.info.name != "Editor":
        print("Validating %s types in %s (%s) %s" % (doc.info.language, doc.info.name, doc.info.path, file))
        errors = []
        warnings = []
        if doc.info.language == "Lua":
            for element in doc.elements:
                for param in element.parameters:
                    for t in param.types:
                        if param.name == "...":
                            continue
                        if not validate_lua_type(t, doc):
                            errors.append("'%s' has unknown type '%s' for parameter '%s' in file %s" % (element.name, t, param.name, file))
                for returnvalue in element.returnvalues:
                    for t in returnvalue.types:
                        if not validate_lua_type(t, doc):
                            errors.append("'%s' has unknown type '%s' for return value '%s' in file %s" % (element.name, t, returnvalue.name, file))
        elif doc.info.language == "C++":
            for element in doc.elements:
                for param in element.parameters:
                    for t in param.types:
                        if t == "":
                            warnings.append("'%s' has no type for parameter '%s' in file %s" % (element.name, param.name, file))
                        elif not validate_cpp_type(t, doc):
                            errors.append("'%s' has unknown type '%s' for parameter '%s' in file %s" % (element.name, t, param.name, file))

        for warning in warnings:
            print("  WARNING", warning)
        for err in errors:
            print("  ERROR", err)
        if len(errors) > 0: raise Exception("Errors occurred when generating documentation")
        # if len(warnings) > 0: sys.exit(1)
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
    d["format_version"] = JSON_FORMAT_VERSION
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


def write_protobuf(msg, output_file):
    with open(output_file, "w") as f:
        f.write(str(msg))


def write_json(msg, output_file):
    dct = message_to_json_dict(msg)
    with open(output_file, "w") as f:
        json.dump(dct, f, indent = 2)


def write_script_api(msg, output_file):
    dct = message_to_yaml_dict(msg)
    with open(output_file, "w") as f:
        yaml.dump(dct, f, default_flow_style = False)


def write_lua_annotation(msg, output_file):
    def fixdoc(s):
        lines = s.splitlines(keepends = True)
        for i,line in enumerate(lines):
            line = re.sub(r'\[icon:.*?\]', r'', line)
            line = re.sub(r'\[type:(.*)\]', r'\1', line)
            line = re.sub(r'^- (.*)', r'\1', line)
            line = re.sub(r'^: (.*)', r'\1', line)
            line = re.sub(r'`(.*?)`', r'\1', line)
            lines[i] = line
        return "---".join(lines)

    if msg.info.language == "Lua":
        dct = message_to_dict(msg)
        dct["info"]["name"] = dct["info"]["name"].replace("-", "_").lower()
        dct["info"]["description"] = re.sub(r'\[icon:.*?\]', r'', dct["info"]["description"])
        for element in dct["elements"]:
            element["is_" + element["type"].lower()] = True
            element["description"] = fixdoc(element["description"])
            element["params_string"] = ", ".join([parameter["name"] for parameter in element["parameters"]])
            element["members_string"] = ", ".join([member["name"] for member in element["members"]])
            for member in element["members"]:
                member["doc"] = fixdoc(member["doc"])
            for parameter in element["parameters"]:
                parameter["types_string"] = "|".join([t for t in parameter["types"]])
                parameter["doc"] = fixdoc(parameter["doc"])
                if parameter["is_optional"] != True:
                    parameter["is_optional"] = None
            for rv in element["returnvalues"]:
                rv["types_string"] = "|".join([t for t in rv["types"]])
                rv["doc"] = fixdoc(rv["doc"])

        result = pystache.render(LUA_MTL, dct)
        result = html.unescape(result)
        with codecs.open(output_file, "wb", encoding="utf-8") as f:
            f.write(result)
    else:
        f = open(output_file, "w")
        f.close()


def write_format(msg, target_format, output_file):
    if target_format == 'protobuf' or target_format == 'sdoc':
        write_protobuf(msg, output_file)
    elif target_format == 'json':
        write_json(msg, output_file)
    elif target_format == 'script_api':
        write_script_api(msg, output_file)
    elif target_format == 'lua':
        write_lua_annotation(msg, output_file)
    else:
        print ('Unknown type: %s' % target_format)
        sys.exit(5)


def write_formats(s, output_specs):
    validation_file = output_specs[0][1] if output_specs else None
    msg = parse_document(s, validation_file)
    for target_format, output_file in output_specs:
        write_format(msg, target_format, output_file)


def to_protobuf(s, output_file):
    msg = parse_document(s, output_file)
    write_protobuf(msg, output_file)


def to_json(s, output_file):
    msg = parse_document(s, output_file)
    write_json(msg, output_file)


def to_script_api(s, output_file):
    msg = parse_document(s, output_file)
    write_script_api(msg, output_file)


def to_lua_annotation(s, output_file):
    msg = parse_document(s, output_file)
    write_lua_annotation(msg, output_file)


if __name__ == '__main__':
    usage = "usage: %prog [options] INFILE(s) OUTFILE"
    parser = OptionParser(usage = usage)
    parser.add_option("-t", "--type", dest="type",
                      help="Supported formats: protobuf, json, script_api and lua. default is protobuf", metavar="TYPE", default='protobuf')
    (options, args) = parser.parse_args()

    if len(args) < 2:
        parser.error('At least two arguments required')

    doc_str = ''
    for name in args[:-1]:
        with open(name, 'r') as f:
            doc_str += f.read()

    output_file = args[-1]
    if options.type == 'protobuf' or options.type == 'sdoc':
        to_protobuf(doc_str, output_file)
    elif options.type == 'json':
        to_json(doc_str, output_file)
    elif options.type == 'script_api':
        to_script_api(doc_str, output_file)
    elif options.type == 'lua':
        to_lua_annotation(doc_str, output_file)
    else:
        print ('Unknown type: %s' % options.type)
        sys.exit(5)
