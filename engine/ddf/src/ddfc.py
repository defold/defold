#!/usr/bin/env python3

LICENSE="""
// Copyright 2020-2024 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.
"""


from optparse import OptionParser

import sys, os, struct
if sys.platform == 'win32':
    import msvcrt
    msvcrt.setmode(sys.stdin.fileno(), os.O_BINARY)
    msvcrt.setmode(sys.stdout.fileno(), os.O_BINARY)

from io import StringIO
import dlib
import functools

from google.protobuf.descriptor import FieldDescriptor
from google.protobuf.descriptor_pb2 import FileDescriptorSet
from google.protobuf.descriptor_pb2 import FieldDescriptorProto

from plugin_pb2 import CodeGeneratorRequest, CodeGeneratorResponse

import ddf.ddf_extensions_pb2

DDF_MAJOR_VERSION=1
DDF_MINOR_VERSION=0

DDF_POINTER_SIZE = 4

type_to_ctype = { FieldDescriptor.TYPE_DOUBLE : "double",
                  FieldDescriptor.TYPE_FLOAT : "float",
                  FieldDescriptor.TYPE_INT64 : "int64_t",
                  FieldDescriptor.TYPE_UINT64 : "uint64_t",
                  FieldDescriptor.TYPE_INT32 : "int32_t",
#                  FieldDescriptor.TYPE_FIXED64
#                  FieldDescriptor.TYPE_FIXED32
                  FieldDescriptor.TYPE_BOOL : "bool",
                  FieldDescriptor.TYPE_STRING : "const char*",
#                  FieldDescriptor.TYPE_GROUP
#                  FieldDescriptor.TYPE_MESSAGE
#                  FieldDescriptor.TYPE_BYTES
                  FieldDescriptor.TYPE_UINT32 : "uint32_t",
#                  FieldDescriptor.TYPE_ENUM
#                  FieldDescriptor.TYPE_SFIXED32
#                  FieldDescriptor.TYPE_SFIXED64
                  FieldDescriptor.TYPE_SINT32 : "int32_t",
                  FieldDescriptor.TYPE_SINT64 : "int64_t" }

type_to_javatype = { FieldDescriptor.TYPE_DOUBLE : "double",
                  FieldDescriptor.TYPE_FLOAT : "float",
                  FieldDescriptor.TYPE_INT64 : "long",
                  FieldDescriptor.TYPE_UINT64 : "long",
                  FieldDescriptor.TYPE_INT32 : "int",
#                  FieldDescriptor.TYPE_FIXED64
#                  FieldDescriptor.TYPE_FIXED32
                  FieldDescriptor.TYPE_BOOL : "boolean",
                  FieldDescriptor.TYPE_STRING : "String",
#                  FieldDescriptor.TYPE_GROUP
#                  FieldDescriptor.TYPE_MESSAGE
#                  FieldDescriptor.TYPE_BYTES
                  FieldDescriptor.TYPE_UINT32 : "int",
#                  FieldDescriptor.TYPE_ENUM
#                  FieldDescriptor.TYPE_SFIXED32
#                  FieldDescriptor.TYPE_SFIXED64
                  FieldDescriptor.TYPE_SINT32 : "int",
                  FieldDescriptor.TYPE_SINT64 : "long" }

type_to_boxedjavatype = { FieldDescriptor.TYPE_DOUBLE : "Double",
                  FieldDescriptor.TYPE_FLOAT : "Float",
                  FieldDescriptor.TYPE_INT64 : "Long",
                  FieldDescriptor.TYPE_UINT64 : "Long",
                  FieldDescriptor.TYPE_INT32 : "Integer",
#                  FieldDescriptor.TYPE_FIXED64
#                  FieldDescriptor.TYPE_FIXED32
                  FieldDescriptor.TYPE_BOOL : "Boolean",
                  FieldDescriptor.TYPE_STRING : "String",
#                  FieldDescriptor.TYPE_GROUP
#                  FieldDescriptor.TYPE_MESSAGE
#                  FieldDescriptor.TYPE_BYTES
                  FieldDescriptor.TYPE_UINT32 : "Integer",
#                  FieldDescriptor.TYPE_ENUM
#                  FieldDescriptor.TYPE_SFIXED32
#                  FieldDescriptor.TYPE_SFIXED64
                  FieldDescriptor.TYPE_SINT32 : "Integer",
                  FieldDescriptor.TYPE_SINT64 : "Long" }

type_to_struct_format = { FieldDescriptor.TYPE_DOUBLE : ("d", float),
                          FieldDescriptor.TYPE_FLOAT : ("f", float),
                          FieldDescriptor.TYPE_INT64 : ("q", int),
                          FieldDescriptor.TYPE_UINT64 : ("Q", int),
                          FieldDescriptor.TYPE_INT32 : ("i", int),
#                         FieldDescriptor.TYPE_FIXED64
#                         FieldDescriptor.TYPE_FIXED32
                          FieldDescriptor.TYPE_BOOL : ("?", bool),
#                         FieldDescriptor.TYPE_STRING  NOTE: String is a special case
#                         FieldDescriptor.TYPE_GROUP
#                         FieldDescriptor.TYPE_MESSAGE
#                         FieldDescriptor.TYPE_BYTES
                          FieldDescriptor.TYPE_UINT32 : ("I", int),
                          FieldDescriptor.TYPE_ENUM : ("i", int),
#                         FieldDescriptor.TYPE_SFIXED32
#                         FieldDescriptor.TYPE_SFIXED64
                          FieldDescriptor.TYPE_SINT32 : ("i", int),
                          FieldDescriptor.TYPE_SINT64 : ("q", int) }

class PrettyPrinter(object):
    def __init__(self, stream, initial_indent = 0):
        self.stream = stream
        self.indent = initial_indent

    def begin(self, str, *format):
        str = str % format
        print (" " * self.indent + str, file=self.stream)
        print (" " * self.indent + "{", file=self.stream)
        self.indent += 4

    def begin_paren(self, str, *format):
        str = str % format
        print (" " * self.indent + str, file=self.stream)
        print (" " * self.indent + "(", file=self.stream)
        self.indent += 4

    def p(self, str, *format):
        str = str % format
        print (" " * self.indent + str, file=self.stream)

    def end(self, str = "", *format):
        str = str % format
        self.indent -= 4
        print (" " * self.indent + "}%s;" % str, file=self.stream)
        print ("", file=self.stream)

    def end_paren(self, str = "", *format):
        str = str % format
        self.indent -= 4
        print (" " * self.indent + ")%s;" % str, file=self.stream)
        print ("", file=self.stream)

def to_camel_case(name, initial_capital = True):
    """Returns a camel-case separated format of the supplied name, which is supposed to be lower-case separated by under-score:
    >>> to_camel_case("abc_abc01_abc")
    "AbcAbc01Abc"
    """
    words = name.lower().split('_')
    for i in range(len(words)):
        if (initial_capital or (i != 0)):
            words[i] = words[i].capitalize()
    return ''.join(words)

def to_lower_case(name):
    """Returns a lower-case under-score separated format of the supplied name, which is supposed to be camel-case:
    >>> ToScriptName("AbcABC01ABCAbc")
    "abc_abc01abc_abc"
    """
    script_name = ""
    for i in range(len(name)):
        if i > 0 and (name[i-1:i].islower() or name[i-1:i].isdigit() or (i+1 < len(name) and name[i+1:i+2].islower())) and name[i:i+1].isupper():
            script_name += "_"
        script_name += name[i:i+1].lower()
    return script_name

def dot_to_cxx_namespace(str):
    if str.startswith("."):
        str = str[1:]
    return str.replace(".", "::")

def to_cxx_struct(context, pp, message_type, namespace):
    # Calculate maximum length of "type"

    namespace = "%s.%s" % (namespace, message_type.name)
    context.set_message_type_defined(dot_to_cxx_namespace(namespace))

    max_len = 0
    for f in message_type.field:
        l = 0
        align_str = ""
        if context.should_align_field(f):
            align_str = "DM_ALIGNED(16) "

        if f.label == FieldDescriptor.LABEL_REPEATED:
            pass
        elif f.type  == FieldDescriptor.TYPE_BYTES:
            pass
        elif f.type == FieldDescriptor.TYPE_ENUM or f.type == FieldDescriptor.TYPE_MESSAGE:
            l += len(align_str + context.get_field_type_name(f))
        else:
            l += len(align_str + type_to_ctype[f.type])

        max_len = max(l, max_len)

    def p(t, n):
        pp.p("%s%sm_%s;", t, (max_len-len(t) + 1) * " ",n)

    if (context.should_align_struct(message_type)):
        pp.begin("struct DM_ALIGNED(16) %s", message_type.name)
    else:
        pp.begin("struct %s", message_type.name)

    for nt in message_type.nested_type:
        pp.p("struct %s;", nt.name)

    for et in message_type.enum_type:
        to_cxx_enum(context, pp, et)

    for nt in message_type.nested_type:
        to_cxx_struct(context, pp, nt, namespace)

    oneof_scope = None

    for f in message_type.field:
        field_name = to_camel_case(f.name)
        field_align = context.should_align_field(f)
        align_str = ""

        if f.HasField("oneof_index"):

            oneof_decl = message_type.oneof_decl[f.oneof_index]

            if oneof_scope == None or oneof_scope != oneof_decl.name:
                if oneof_scope != None:
                    pp.end(" m_%s", to_camel_case(oneof_scope))
                    pp.p("uint8_t m_%sOneOfIndex;", to_camel_case(oneof_scope))

                oneof_scope = oneof_decl.name
                pp.begin("union")
        else:
            if oneof_scope != None:
                pp.end(" m_%s", to_camel_case(oneof_scope))
                pp.p("uint8_t m_%sOneOfIndex;", to_camel_case(oneof_scope))
                oneof_scope = None

        if (field_align):
            align_str = "DM_ALIGNED(16) "
        if f.label == FieldDescriptor.LABEL_REPEATED or f.type == FieldDescriptor.TYPE_BYTES:
            if (field_align):
                pp.begin("struct DM_ALIGNED(16)")
            else:
                pp.begin("struct")

            if f.type ==  FieldDescriptor.TYPE_MESSAGE:
                type_name = dot_to_cxx_namespace(f.type_name)
            elif f.type ==  FieldDescriptor.TYPE_BYTES:
                type_name = align_str + "uint8_t"
            else:
                type_name = align_str + type_to_ctype[f.type]

            pp.p(type_name+"* m_Data;")

            if f.type == FieldDescriptor.TYPE_STRING:
                pp.p("%s operator[](uint32_t i) const { assert(i < m_Count); return m_Data[i]; }", type_name)
            else:
                pp.p("const %s& operator[](uint32_t i) const { assert(i < m_Count); return m_Data[i]; }", type_name)
                pp.p("%s& operator[](uint32_t i) { assert(i < m_Count); return m_Data[i]; }", type_name)

            pp.p("uint32_t " + "m_Count;")
            pp.end(" m_%s", field_name)
        elif f.type ==  FieldDescriptor.TYPE_ENUM or f.type == FieldDescriptor.TYPE_MESSAGE:

            field_ptr_str = ""

            if f.type == FieldDescriptor.TYPE_MESSAGE and not context.get_is_message_type_defined(f):
                field_ptr_str = "*"

            p(align_str + context.get_field_type_name(f) + field_ptr_str, field_name)
        else:
            p(align_str + type_to_ctype[f.type], field_name)

    if oneof_scope != None:
        pp.end(" m_%s", to_camel_case(oneof_scope))

        pp.p("uint8_t m_%sOneOfIndex;", to_camel_case(oneof_scope))
        pp.p("")

    pp.p('')
    pp.p('static dmDDF::Descriptor* m_DDFDescriptor;')
    pp.p('static const uint64_t m_DDFHash;')
    pp.end()

def to_cxx_enum(context, pp, message_type):
    lst = []
    for f in message_type.value:
        lst.append((f.name, f.number))

    max_len = functools.reduce(lambda y,x: max(len(x[0]), y), lst, 0)
    pp.begin("enum %s",  message_type.name)

    for t,n in lst:
        pp.p("%s%s= %d,", t, (max_len-len(t) + 1) * " ",n)

    pp.end()

def to_cxx_default_value_string(context, f):
    # Skip all empty values. Type string is an exception as we always set these to ""
    if len(f.default_value) == 0 and f.type != FieldDescriptor.TYPE_STRING:
        return ''
    else:
        if f.type == FieldDescriptor.TYPE_STRING:
            return '"%s\\x00"' % ''.join(map(lambda x: '\\x%02x' % ord(x), f.default_value))
        elif f.type == FieldDescriptor.TYPE_ENUM:
            default_value = None
            for e in context.message_types[f.type_name].value:
                if e.name == f.default_value:
                    default_value = e.number
                    break
            if default_value == None:
                raise Exception("Default '%s' not found" % f.default_value)

            form, func = type_to_struct_format[f.type]
            # Store in little endian
            tmp = struct.pack('<' + form, func(default_value))
            return '"%s"' % ''.join(map(lambda x: '\\x%02x' % ord(chr(x)), tmp))
        elif f.type == FieldDescriptor.TYPE_BOOL:
            if f.default_value == "true":
                return '"\\x01"'
            else:
                return '"\\x00"'
        else:
            form, func = type_to_struct_format[f.type]
            # Store in little endian
            tmp = struct.pack('<' + form, func(f.default_value))
            return '"%s"' % ''.join(map(lambda x: '\\x%02x' % ord(chr(x)), tmp))

def to_cxx_descriptor(context, cpp_desc_map, pp_cpp, pp_h, message_type, namespace_lst):
    namespace = "_".join(namespace_lst)
    pp_h.p('extern dmDDF::Descriptor %s_%s_DESCRIPTOR;', namespace, message_type.name)

    mt_type_name = "%s.%s" % ("::".join(namespace_lst), message_type.name)

    context.set_message_type_defined(dot_to_cxx_namespace(mt_type_name))

    for nt in message_type.nested_type:
        to_cxx_descriptor(context, cpp_desc_map, pp_cpp, pp_h, nt, namespace_lst + [message_type.name] )

    for f in message_type.field:
        default = to_cxx_default_value_string(context, f)
        if '"' in default:
            pp_cpp.p('char DM_ALIGNED(4) %s_%s_%s_DEFAULT_VALUE[] = %s;', namespace, message_type.name, f.name, default)

    contains_dynamic_fields = needs_size_resolve(context, message_type, cpp_desc_map)

    oneof_scope_names = []
    oneof_scope = None
    lst = []
    for f in message_type.field:
        one_of_index = 0
        fully_defined_type = True

        if f.HasField("oneof_index"):
            oneof_decl = message_type.oneof_decl[f.oneof_index]

            # indices start at zero, but we need to distinguish between which fields belong
            # to a oneof block somehow
            one_of_index = f.oneof_index + 1

            if oneof_scope == None or oneof_scope != oneof_decl.name:
                oneof_scope = oneof_decl.name
                oneof_scope_names.append(to_camel_case(oneof_decl.name))
        else:
            if oneof_scope != None:
                oneof_scope = None

        tpl = (f.name, f.number, f.type, f.label, one_of_index)
        if f.type ==  FieldDescriptor.TYPE_MESSAGE:
            fully_defined_type = context.get_is_message_type_defined(f)

            tmp = f.type_name.replace(".", "_")
            if tmp.startswith("_"):
                tmp = tmp[1:]
            tpl += ("&" + tmp + "_DESCRIPTOR", )
        else:
            tpl += ("0x0", )

        oneof_scope_name = ""
        if oneof_scope != None:
            oneof_scope_name = "m_%s." % to_camel_case(oneof_scope)

        tpl += ("(uint32_t)DDF_OFFSET_OF(%s::%s, %sm_%s)" % (namespace.replace("_", "::"), message_type.name, oneof_scope_name, to_camel_case(f.name)), )

        default = to_cxx_default_value_string(context, f)
        if '"' in default:
            tpl += ('%s_%s_%s_DEFAULT_VALUE' % (namespace, message_type.name, f.name), )
        else:
            tpl += ('0x0',)

        tpl += (fully_defined_type, )

        lst.append(tpl)

    if len(lst) > 0:
        pp_cpp.begin("dmDDF::FieldDescriptor %s_%s_FIELDS_DESCRIPTOR[] = ", namespace, message_type.name)

        for name, number, type, label, one_of_index, msg_desc, offset, default_value, fully_defined_type in lst:
            pp_cpp.p('{ "%s", %d, %d, %d, %s, %s, %s, %d, %d},'  % (name, number, type, label, msg_desc, offset, default_value, one_of_index, fully_defined_type))

        pp_cpp.end()
    else:
        pp_cpp.p("dmDDF::FieldDescriptor* %s_%s_FIELDS_DESCRIPTOR = 0x0;", namespace, message_type.name)

    if len(oneof_scope_names) > 0:
        pp_cpp.p('uint32_t %s_%s_ONEOF_OFFSETS[] = {', namespace, message_type.name)
        for name in oneof_scope_names:
            pp_cpp.p('    (uint32_t)DDF_OFFSET_OF(%s::%s, m_%sOneOfIndex),' % (namespace.replace("_", "::"), message_type.name, name))
        pp_cpp.p('};')
    else:
        pp_cpp.p('uint32_t* %s_%s_ONEOF_OFFSETS = 0x0;', namespace, message_type.name)

    pp_cpp.begin("dmDDF::Descriptor %s_%s_DESCRIPTOR = ", namespace, message_type.name)
    pp_cpp.p('%d, %d,', DDF_MAJOR_VERSION, DDF_MINOR_VERSION)
    pp_cpp.p('"%s",', to_lower_case(message_type.name))
    pp_cpp.p('0x%016XULL,', dlib.dmHashBuffer64(to_lower_case(message_type.name)))
    pp_cpp.p('sizeof(%s::%s),', namespace.replace("_", "::"), message_type.name)

    # Descriptors
    pp_cpp.p('%s_%s_FIELDS_DESCRIPTOR,', namespace, message_type.name)
    if len(lst) > 0:
        pp_cpp.p('sizeof(%s_%s_FIELDS_DESCRIPTOR)/sizeof(dmDDF::FieldDescriptor),', namespace, message_type.name)
    else:
        pp_cpp.p('0,')

    # One-of offsets
    pp_cpp.p('%s_%s_ONEOF_OFFSETS,', namespace, message_type.name)
    if len(oneof_scope_names) > 0:
        pp_cpp.p('sizeof(%s_%s_ONEOF_OFFSETS)/sizeof(uint32_t),', namespace, message_type.name)
    else:
        pp_cpp.p('0,')

    # Contains dynamic fields
    pp_cpp.p('%d,', contains_dynamic_fields and 1 or 0)

    pp_cpp.end()

    pp_cpp.p('dmDDF::Descriptor* %s::%s::m_DDFDescriptor = &%s_%s_DESCRIPTOR;' % ('::'.join(namespace_lst), message_type.name, namespace, message_type.name))

    # TODO: This is not optimal. Hash value is sensitive on googles format string
    # Also dependent on type invariant values?
    hash_string = str(message_type).replace(" ", "").replace("\n", "").replace("\r", "")
    pp_cpp.p('const uint64_t %s::%s::m_DDFHash = 0x%016XULL;' % ('::'.join(namespace_lst), message_type.name, dlib.dmHashBuffer64(hash_string)))

    pp_cpp.p('dmDDF::InternalRegisterDescriptor g_Register_%s_%s(&%s_%s_DESCRIPTOR);' % (namespace, message_type.name, namespace, message_type.name))

    pp_cpp.p('')

def to_cxx_enum_descriptor(context, pp_cpp, pp_h, enum_type, namespace_lst):
    namespace = "_".join(namespace_lst)

    pp_h.p("extern dmDDF::EnumDescriptor %s_%s_DESCRIPTOR;", namespace, enum_type.name)

    lst = []
    for f in enum_type.value:
        tpl = (f.name, f.number)
        lst.append(tpl)

    pp_cpp.begin("dmDDF::EnumValueDescriptor %s_%s_FIELDS_DESCRIPTOR[] = ", namespace, enum_type.name)

    for name, number in lst:
        pp_cpp.p('{ "%s", %d },'  % (name, number))

    pp_cpp.end()

    pp_cpp.begin("dmDDF::EnumDescriptor %s_%s_DESCRIPTOR = ", namespace, enum_type.name)
    pp_cpp.p('%d, %d,', DDF_MAJOR_VERSION, DDF_MINOR_VERSION)
    pp_cpp.p('"%s",', to_lower_case(enum_type.name))
    pp_cpp.p('%s_%s_FIELDS_DESCRIPTOR,', namespace, enum_type.name)
    pp_cpp.p('sizeof(%s_%s_FIELDS_DESCRIPTOR)/sizeof(dmDDF::EnumValueDescriptor),', namespace, enum_type.name)
    pp_cpp.end()

def dot_to_java_package(context, str, proto_package, java_package):
    if str.startswith("."):
        str = str[1:]

    ret = context.type_name_to_java_type[str]
    ret = ret.replace(java_package + '.', "")
    return ret

def to_java_enum(context, pp, message_type):
    lst = []
    for f in message_type.value:
        lst.append(("public static final int %s" % (f.name), f.number))

    max_len = reduce(lambda y,x: max(len(x[0]), y), lst, 0)
    #pp.begin("enum %s",  message_type.name)

    for t,n in lst:
        pp.p("%s%s= %d;", t, (max_len-len(t) + 1) * " ",n)
    pp.p("")
    #pp.end()

def to_java_enum_descriptor(context, pp, enum_type, qualified_proto_package):

    lst = []
    for f in enum_type.value:
        tpl = (f.name, f.number)
        lst.append(tpl)

    pp.begin('public static class %s' % enum_type.name)
    pp.begin("public static com.dynamo.ddf.EnumValueDescriptor VALUES_DESCRIPTOR[] = new com.dynamo.ddf.EnumValueDescriptor[]")

    for name, number in lst:
        pp.p('new com.dynamo.ddf.EnumValueDescriptor("%s", %d),'  % (name, number))

    pp.end()
    name = qualified_proto_package.replace('$', '_').replace('.', '_')
    pp.p('public static com.dynamo.ddf.EnumDescriptor DESCRIPTOR = new com.dynamo.ddf.EnumDescriptor("%s", VALUES_DESCRIPTOR);' % (name))
    pp.end()

def to_java_descriptor(context, pp, message_type, proto_package, java_package, qualified_proto_package):

    lst = []
    for f in message_type.field:
        tpl = (to_camel_case(f.name, False), f.number, f.type, f.label)
        f_type_name = f.type_name
        if f.type ==  FieldDescriptor.TYPE_MESSAGE:
            tmp = dot_to_java_package(context, f_type_name, proto_package, java_package)
            if tmp.startswith("."):
                tmp = tmp[1:]
            tpl += (tmp + ".DESCRIPTOR", "null")
        elif f.type ==  FieldDescriptor.TYPE_ENUM:
            tmp = dot_to_java_package(context, f_type_name, proto_package, java_package)
            if tmp.startswith("."):
                tmp = tmp[1:]
            tpl += ("null", tmp + ".DESCRIPTOR", )

        else:
            tpl += ("null", "null")
        lst.append(tpl)

    pp.begin("public static com.dynamo.ddf.FieldDescriptor FIELDS_DESCRIPTOR[] = new com.dynamo.ddf.FieldDescriptor[]")

    for name, number, type, label, msg_desc, enum_desc in lst:
        pp.p('new com.dynamo.ddf.FieldDescriptor("%s", %d, %d, %d, %s, %s),'  % (name, number, type, label, msg_desc, enum_desc))

    pp.end()

    pp.begin_paren("public static com.dynamo.ddf.Descriptor DESCRIPTOR = new com.dynamo.ddf.Descriptor")
    pp.p('"%s",', qualified_proto_package.replace('$', '_').replace('.', '_'))
    pp.p('FIELDS_DESCRIPTOR')
    pp.end_paren('')

    # TODO: Add hash support for java types?
    # TODO: This is not optimal. Hash value is sensitive on googles format string
    # Also dependent on type invariant values?
    hash_string = str(message_type).replace(" ", "").replace("\n", "").replace("\r", "")
    #pp.p('const uint64_t %s::%s::m_DDFHash = 0x%016XULL;' % ('::'.join(namespace_lst), message_type.name, dlib.dmHashBuffer64(hash_string)))

def to_java_class(context, pp, message_type, proto_package, java_package, qualified_proto_package):
    # Calculate maximum length of "type"
    max_len = 0
    for f in message_type.field:
        if f.label == FieldDescriptor.LABEL_REPEATED:
            if f.type ==  FieldDescriptor.TYPE_MESSAGE:
                tn = dot_to_java_package(context, f.type_name, proto_package, java_package)
            elif f.type == FieldDescriptor.TYPE_BYTES:
                assert False
            else:
                tn = type_to_boxedjavatype[f.type]
            max_len = max(len('List<%s>' % tn), max_len)
        elif f.type  == FieldDescriptor.TYPE_BYTES:
            max_len = max(len("byte[]"), max_len)
        elif f.type == FieldDescriptor.TYPE_ENUM:
            max_len = max(len("int"), max_len)
        elif f.type == FieldDescriptor.TYPE_MESSAGE:
            max_len = max(len(dot_to_java_package(context, f.type_name, proto_package, java_package)), max_len)
        else:
            max_len = max(len(type_to_javatype[f.type]), max_len)

    def p(t, n, assign = None):
        if assign:
            pp.p("public %s%s%s = %s;", t, (max_len-len(t) + 1) * " ", n, assign)
        else:
            pp.p("public %s%s%s;", t, (max_len-len(t) + 1) * " ", n)

    pp.p('@ProtoClassName(name = "%s")' % qualified_proto_package)
    pp.begin("public static final class %s", message_type.name)

    for et in message_type.enum_type:
        to_java_enum_descriptor(context, pp, et, qualified_proto_package + '_' + et.name)
        to_java_enum(context, pp, et)

    to_java_descriptor(context, pp, message_type, proto_package, java_package, qualified_proto_package)

    for nt in message_type.nested_type:
        to_java_class(context, pp, nt, proto_package, java_package, qualified_proto_package + "$" + nt.name)

    for f in message_type.field:
        f_name = to_camel_case(f.name, False)
        if f.label == FieldDescriptor.LABEL_REPEATED:
            if f.type ==  FieldDescriptor.TYPE_MESSAGE:
                type_name = dot_to_java_package(context, f.type_name, proto_package, java_package)
            elif f.type ==  FieldDescriptor.TYPE_BYTES:
                type_name = "Byte"
            else:
                type_name = type_to_boxedjavatype[f.type]

            pp.p('@ComponentType(type = %s.class)' % type_name)
            p('List<%s>' % (type_name), f_name, 'new ArrayList<%s>()' % type_name)
        elif f.type == FieldDescriptor.TYPE_BYTES:
            p('byte[]', f_name)
        elif f.type == FieldDescriptor.TYPE_ENUM:
            p("int", f_name)
        elif f.type == FieldDescriptor.TYPE_MESSAGE:
            java_type_name = dot_to_java_package(context, f.type_name, proto_package, java_package)
            p(java_type_name, f_name, 'new %s()' % java_type_name)
        else:
            p(type_to_javatype[f.type], f_name)
    pp.end()

def compile_java(context, proto_file, ddf_java_package, file_to_generate):
    file_desc = proto_file

    path = ddf_java_package.replace('.', '/')


    file_java = context.response.file.add()
    file_java.name = os.path.join(path, proto_file.options.java_outer_classname + '.java')
    f_java = StringIO()

    pp_java = PrettyPrinter(f_java, 0)

    if ddf_java_package != '':
        pp_java.p("package %s;",  ddf_java_package)
    pp_java.p("")
    pp_java.p("import java.util.List;")
    pp_java.p("import java.util.ArrayList;")
    pp_java.p("import com.dynamo.ddf.annotations.ComponentType;")
    pp_java.p("import com.dynamo.ddf.annotations.ProtoClassName;")

    pp_java.p("")
    pp_java.begin("public final class %s", proto_file.options.java_outer_classname)

    for mt in file_desc.enum_type:
        to_java_enum_descriptor(context, pp_java, mt, ddf_java_package + '_' + mt.name)
        to_java_enum(context, pp_java, mt)

    for mt in file_desc.message_type:
        to_java_class(context, pp_java, mt, file_desc.package,
                    ddf_java_package + "." + proto_file.options.java_outer_classname,
                    file_desc.options.java_package + "." + file_desc.options.java_outer_classname + "$" + mt.name)

    pp_java.end("")

    file_java.content = f_java.getvalue()

def to_ensure_struct_alias_size(context, file_desc, pp_cpp):
    import hashlib
    m = hashlib.md5((file_desc.package + file_desc.name).encode('ascii'))
    pp_cpp.begin('void EnsureStructAliasSize_%s()' % m.hexdigest())

    for t, at in context.type_alias_messages.items():
        pp_cpp.p('DM_STATIC_ASSERT(sizeof(%s) == sizeof(%s), Invalid_Struct_Alias_Size);' % (dot_to_cxx_namespace(t), at))

    pp_cpp.end()

def needs_size_resolve(context, message_type, cpp_desc_map, visiting=None, cache=None):
    """
    Returns True if the message requires dynamic sizing.
    Conditions:
    - Contains a non-repeated message field whose type is not fully defined
    - Contains a recursive reference (cycle)
    """
    if visiting is None:
        visiting = set()
    if cache is None:
        cache = {}

    mid = id(message_type)

    # Cached result?
    if mid in cache:
        return cache[mid]

    # Cycle detected → dynamic!
    if mid in visiting:
        cache[mid] = True
        return True

    visiting.add(mid)

    for f in message_type.field:
        if f.label != FieldDescriptorProto.LABEL_REPEATED and f.type == FieldDescriptorProto.TYPE_MESSAGE:

            # Is it fully defined?
            try:
                fully_defined = context.get_is_message_type_defined(f)
            except:
                fully_defined = False

            if not fully_defined:
                cache[mid] = True
                visiting.remove(mid)
                return True

            # Look up nested type
            cpp_type = context.get_field_type_name(f)
            nested_desc = cpp_desc_map.get(cpp_type)

            if nested_desc:
                if needs_size_resolve(context, nested_desc, cpp_desc_map, visiting, cache):
                    cache[mid] = True
                    visiting.remove(mid)
                    return True

    visiting.remove(mid)
    cache[mid] = False
    return False

def build_cpp_desc_map_for_file(file_desc, package):
    # Build mapping: C++ type name (e.g. "my.pkg::Msg") -> DescriptorProto
    # We keep it simple and only map top-level / nested types declared in this file.
    cpp_map = {}

    def walk(mt, ns_lst):
        cpp_name = ("::".join(ns_lst + [mt.name]))
        cpp_map[cpp_name] = mt
        # Recurse nested types
        if hasattr(mt, "nested_type"):
            for nested in mt.nested_type:
                walk(nested, ns_lst + [mt.name])

    base_ns = package

    # If base_ns is something like "pkg.name", split on '.' into "pkg::name".
    ns_list = []
    if base_ns:
        # create namespace as C++ style components: "a.b" -> ["a", "b"]
        ns_list = base_ns.split(".")
    for mt in file_desc.message_type:
        walk(mt, ns_list)

    return cpp_map

def compile_cxx(context, proto_file, file_to_generate, namespace, includes):
    base_name = os.path.basename(file_to_generate)

    if base_name.rfind(".") != -1:
        base_name = base_name[0:base_name.rfind(".")]

    file_desc = proto_file

    # Helper: mark all message types from other proto files as defined
    def mark_imported_types():
        for type_name, origin in context.message_type_file.items():
            if origin != file_to_generate:
                # Strip leading '.' (proto full names start with dot)
                clean_name = type_name[1:] if type_name.startswith('.') else type_name
                # Convert "dmMath.Point3" -> "dmMath::Point3"
                cxx_type = dot_to_cxx_namespace(clean_name)
                context.set_message_type_defined(cxx_type)

    # First pass: mark imported types so struct generation uses correct layouts
    mark_imported_types()

    file_h = context.response.file.add()
    file_h.name = base_name + ".h"

    f_h = StringIO()
    pp_h = PrettyPrinter(f_h, 0)

    pp_h.p(LICENSE)
    pp_h.p("")
    pp_h.p("// GENERATED FILE! DO NOT EDIT!");
    pp_h.p("")

    guard_name = base_name.upper() + "_H"
    pp_h.p('#ifndef %s', guard_name)
    pp_h.p('#define %s', guard_name)
    pp_h.p("")

    pp_h.p('#include <stdint.h>')
    pp_h.p('#include <assert.h>')
    pp_h.p('#include <ddf/ddf.h>')
    for d in file_desc.dependency:
        if not 'ddf_extensions' in d:
            pp_h.p('#include "%s"', d.replace(".proto", ".h"))
    pp_h.p('#include <dmsdk/dlib/align.h>')

    for i in includes:
        pp_h.p('#include "%s"', i)
    pp_h.p("")

    if namespace:
        pp_h.begin("namespace %s",  namespace)
    pp_h.begin("namespace %s",  file_desc.package)

    # forward declare base message types
    for mt in file_desc.message_type:
        pp_h.p("struct %s;", mt.name)
    pp_h.p("")

    for mt in file_desc.enum_type:
        to_cxx_enum(context, pp_h, mt)

    for mt in file_desc.message_type:
        to_cxx_struct(context, pp_h, mt, file_desc.package)

    pp_h.end()

    file_cpp = context.response.file.add()
    file_cpp.name = base_name + ".cpp"
    f_cpp = StringIO()

    pp_cpp = PrettyPrinter(f_cpp, 0)
    pp_cpp.p('#include <ddf/ddf.h>')
    for d in file_desc.dependency:
        if not 'ddf_extensions' in d:
            pp_cpp.p('#include "%s"', d.replace(".proto", ".h"))
    pp_cpp.p('#include "%s.h"' % base_name)

    if namespace:
        pp_cpp.begin("namespace %s",  namespace)

    pp_h.p("#ifdef DDF_EXPOSE_DESCRIPTORS")

    context.reset_defined_message_types()

    mark_imported_types()

    for mt in file_desc.enum_type:
        to_cxx_enum_descriptor(context, pp_cpp, pp_h, mt, [file_desc.package])

    # Build mapping of C++ type to descriptor
    cpp_desc_map = build_cpp_desc_map_for_file(file_desc, file_desc.package)

    for mt in file_desc.message_type:
        to_cxx_descriptor(context, cpp_desc_map, pp_cpp, pp_h, mt, [file_desc.package])

    pp_h.p("#endif")

    if namespace:
        pp_h.end()

    to_ensure_struct_alias_size(context, file_desc, pp_cpp)

    if namespace:
        pp_cpp.end()

    file_cpp.content = f_cpp.getvalue()

    pp_h.p('#endif // %s ', guard_name)
    file_h.content = f_h.getvalue()


class CompilerContext(object):
    def __init__(self, request):
        self.request = request
        self.message_types = {}
        self.message_type_file = {}
        self.defined_message_types = {}
        self.type_name_to_java_type = {}
        self.type_alias_messages = {}
        self.response = CodeGeneratorResponse()

    # TODO: We add enum types as message types. Kind of hack...
    def add_message_type(self, package, java_package, java_outer_classname, message_type, origin_filename):
        """
        Register a (possibly nested) message/enum type and record the .proto filename
        it originates from. 'package' is expected to be the same format you used
        elsewhere (you currently pass '.' + pf.package for top-level calls).
        """
        # Build fully-qualified key exactly like other callers expect:
        n = str(package + '.' + message_type.name)

        # If we've already registered this exact key, stop (prevents duplicates).
        if n in self.message_types:
            return

        # Store the message/enum descriptor under the fully-qualified key.
        self.message_types[n] = message_type
        self.message_type_file[n] = origin_filename   # store origin filename for pre-marking

        # If the type is aliased, remember that too
        if self.has_type_alias(n):
            self.type_alias_messages[n] = self.type_alias_name(n)

        # Java type mapping (keeps the original logic)
        # Note: package may start with a dot; original code used package[1:].
        # Guard small cases where java_package might be '' or None.
        jp = java_package if java_package is not None else ''
        jouter = java_outer_classname if java_outer_classname is not None else ''
        self.type_name_to_java_type[package[1:] + '.' + message_type.name] = jp + '.' + jouter + '.' + message_type.name

        # Recurse nested messages and enums, passing the same origin filename
        if hasattr(message_type, 'nested_type'):
            for mt in message_type.nested_type:
                self.add_message_type(package + '.' + message_type.name,
                                      java_package,
                                      java_outer_classname,
                                      mt,
                                      origin_filename)

        if hasattr(message_type, 'enum_type'):
            for et in message_type.enum_type:
                self.add_message_type(package + '.' + message_type.name,
                                      java_package,
                                      java_outer_classname,
                                      et,
                                      origin_filename)

    def should_align_field(self, f):
        for x in f.options.ListFields():
            if x[0].name == 'field_align':
                return True
        return False

    def should_align_struct(self, mt):
        for x in mt.options.ListFields():
            if x[0].name == 'struct_align':
                return True
        return False

    def has_type_alias(self, type_name):
        mt = self.message_types[type_name]
        for x in mt.options.ListFields():
            if x[0].name == 'alias':
                return True
        return False

    def type_alias_name(self, type_name):
        mt = self.message_types[type_name]
        for x in mt.options.ListFields():
            if x[0].name == 'alias':
                return x[1]
        assert False

    def set_message_type_defined(self, type_name):
        self.defined_message_types[type_name] = True

    def get_is_message_type_defined(self, f):
        type_name = dot_to_cxx_namespace(f.type_name)
        if type_name in self.defined_message_types:
            return True
        return False

    def reset_defined_message_types(self):
        self.defined_message_types = {}

    def get_field_type_name(self, f):
        if f.type ==  FieldDescriptor.TYPE_MESSAGE:
            if self.has_type_alias(f.type_name):
                return self.type_alias_name(f.type_name)
            else:
                return dot_to_cxx_namespace(f.type_name)
        elif f.type == FieldDescriptor.TYPE_ENUM:
            return dot_to_cxx_namespace(f.type_name)
        else:
            assert(False)

if __name__ == '__main__':
    import doctest

    usage = "usage: %prog [options] FILE"
    parser = OptionParser(usage = usage)
    parser.add_option("-o", dest="output_file", help="Output file (.cpp)", metavar="FILE")
    parser.add_option("--java", dest="java", help="Generate java", action="store_true")
    parser.add_option("--cxx", dest="cxx", help="Generate c++", action="store_true")
    (options, args) = parser.parse_args()

    request = CodeGeneratorRequest()
    request.ParseFromString(sys.stdin.buffer.read())
    context = CompilerContext(request)

    for pf in request.proto_file:
        java_package = ''
        for x in pf.options.ListFields():
            if x[0].name == 'ddf_java_package':
                java_package = x[1]

        for mt in pf.message_type:
            context.add_message_type('.' + pf.package, java_package, pf.options.java_outer_classname, mt, pf.name)

        for et in pf.enum_type:
            # NOTE: We add enum types as message types. Kind of hack...
            context.add_message_type('.' + pf.package, java_package, pf.options.java_outer_classname, et, pf.name)

    for pf in request.proto_file:
        if pf.name == request.file_to_generate[0]:
            namespace = None
            includes = []

            for x in pf.options.ListFields():
                if x[0].name == 'ddf_includes':
                    includes = x[1].split()
                elif x[0].name == 'ddf_namespace':
                    namespace = x[1]

            if options.cxx:
                compile_cxx(context, pf, request.file_to_generate[0], namespace, includes)

            for x in pf.options.ListFields():
                if x[0].name == 'ddf_java_package':
                    if options.java:
                        compile_java(context, pf, x[1], request.file_to_generate[0])

    sys.stdout.buffer.write(context.response.SerializeToString())
    # res = str(context.response.SerializeToString())
