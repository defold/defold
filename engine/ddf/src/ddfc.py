#!/usr/bin/python

from optparse import OptionParser

import sys, os, struct
if sys.platform == 'win32':
    import msvcrt
    msvcrt.setmode(sys.stdin.fileno(), os.O_BINARY)
    msvcrt.setmode(sys.stdout.fileno(), os.O_BINARY)

from cStringIO import StringIO
import dlib

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
        print >>self.stream, " " * self.indent + str
        print >>self.stream, " " * self.indent + "{"
        self.indent += 4

    def begin_paren(self, str, *format):
        str = str % format
        print >>self.stream, " " * self.indent + str
        print >>self.stream, " " * self.indent + "("
        self.indent += 4

    def p(self, str, *format):
        str = str % format
        print >>self.stream, " " * self.indent + str

    def end(self, str = "", *format):
        str = str % format
        self.indent -= 4
        print >>self.stream, " " * self.indent + "}%s;" % str
        print >>self.stream, ""

    def end_paren(self, str = "", *format):
        str = str % format
        self.indent -= 4
        print >>self.stream, " " * self.indent + ")%s;" % str
        print >>self.stream, ""

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

def to_cxx_struct(context, pp, message_type):
    # Calculate maximum length of "type"
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

    for et in message_type.enum_type:
        to_cxx_enum(context, pp, et)

    for nt in message_type.nested_type:
        to_cxx_struct(context, pp, nt)

    for f in message_type.field:
        field_name = to_camel_case(f.name)
        field_align = context.should_align_field(f)
        align_str = ""
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
            p(align_str + context.get_field_type_name(f), field_name)
        else:
            p(align_str + type_to_ctype[f.type], field_name)
    pp.p('')
    pp.p('static dmDDF::Descriptor* m_DDFDescriptor;')
    pp.p('static const uint64_t m_DDFHash;')
    pp.end()

def to_cxx_enum(context, pp, message_type):
    lst = []
    for f in message_type.value:
        lst.append((f.name, f.number))

    max_len = reduce(lambda y,x: max(len(x[0]), y), lst, 0)
    pp.begin("enum %s",  message_type.name)

    for t,n in lst:
        pp.p("%s%s= %d,", t, (max_len-len(t) + 1) * " ",n)

    pp.end()

def to_cxx_default_value_string(context, f):
    # Skip all empty values. Type string is an exception as we always set these to ""
    if len(f.default_value) == 0 and f.type != FieldDescriptor.TYPE_STRING:
        return '0x0'
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
            return '"%s"' % ''.join(map(lambda x: '\\x%02x' % ord(x), tmp))
        elif f.type == FieldDescriptor.TYPE_BOOL:
            if f.default_value == "true":
                return '"\x01"'
            else:
                return '"\x00"'
        else:
            form, func = type_to_struct_format[f.type]
            # Store in little endian
            tmp = struct.pack('<' + form, func(f.default_value))
            return '"%s"' % ''.join(map(lambda x: '\\x%02x' % ord(x), tmp))

def to_cxx_descriptor(context, pp_cpp, pp_h, message_type, namespace_lst):
    namespace = "_".join(namespace_lst)
    pp_h.p('extern dmDDF::Descriptor %s_%s_DESCRIPTOR;', namespace, message_type.name)

    for nt in message_type.nested_type:
        to_cxx_descriptor(context, pp_cpp, pp_h, nt, namespace_lst + [message_type.name] )

    for f in message_type.field:
        default = to_cxx_default_value_string(context, f)
        if '"' in default:
            pp_cpp.p('char DM_ALIGNED(4) %s_%s_%s_DEFAULT_VALUE[] = %s;', namespace, message_type.name, f.name, default)

    lst = []
    for f in message_type.field:
        tpl = (f.name, f.number, f.type, f.label)
        if f.type ==  FieldDescriptor.TYPE_MESSAGE:
            tmp = f.type_name.replace(".", "_")
            if tmp.startswith("_"):
                tmp = tmp[1:]
            tpl += ("&" + tmp + "_DESCRIPTOR", )
        else:
            tpl += ("0x0", )

        tpl += ("(uint32_t)DDF_OFFSET_OF(%s::%s, m_%s)" % (namespace.replace("_", "::"), message_type.name, to_camel_case(f.name)), )

        default = to_cxx_default_value_string(context, f)
        if '"' in default:
            tpl += ('%s_%s_%s_DEFAULT_VALUE' % (namespace, message_type.name, f.name), )
        else:
            tpl += ('0x0',)

        lst.append(tpl)

    if len(lst) > 0:
        pp_cpp.begin("dmDDF::FieldDescriptor %s_%s_FIELDS_DESCRIPTOR[] = ", namespace, message_type.name)
        for name, number, type, label, msg_desc, offset, default_value in lst:
            pp_cpp.p('{ "%s", %d, %d, %d, %s, %s, %s},'  % (name, number, type, label, msg_desc, offset, default_value))
        pp_cpp.end()
    else:
        pp_cpp.p("dmDDF::FieldDescriptor* %s_%s_FIELDS_DESCRIPTOR = 0x0;", namespace, message_type.name)

    pp_cpp.begin("dmDDF::Descriptor %s_%s_DESCRIPTOR = ", namespace, message_type.name)
    pp_cpp.p('%d, %d,', DDF_MAJOR_VERSION, DDF_MINOR_VERSION)
    pp_cpp.p('"%s",', to_lower_case(message_type.name))
    pp_cpp.p('0x%016XLL,', dlib.dmHashBuffer64(to_lower_case(message_type.name)))
    pp_cpp.p('sizeof(%s::%s),', namespace.replace("_", "::"), message_type.name)
    pp_cpp.p('%s_%s_FIELDS_DESCRIPTOR,', namespace, message_type.name)
    if len(lst) > 0:
        pp_cpp.p('sizeof(%s_%s_FIELDS_DESCRIPTOR)/sizeof(dmDDF::FieldDescriptor),', namespace, message_type.name)
    else:
        pp_cpp.p('0,')
    pp_cpp.end()

    pp_cpp.p('dmDDF::Descriptor* %s::%s::m_DDFDescriptor = &%s_%s_DESCRIPTOR;' % ('::'.join(namespace_lst), message_type.name, namespace, message_type.name))

    # TODO: This is not optimal. Hash value is sensitive on googles format string
    # Also dependent on type invariant values?
    hash_string = str(message_type).replace(" ", "").replace("\n", "").replace("\r", "")
    pp_cpp.p('const uint64_t %s::%s::m_DDFHash = 0x%016XLL;' % ('::'.join(namespace_lst), message_type.name, dlib.dmHashBuffer64(hash_string)))

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
    #pp.p('const uint64_t %s::%s::m_DDFHash = 0x%016XLL;' % ('::'.join(namespace_lst), message_type.name, dlib.dmHashBuffer64(hash_string)))

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
    m = hashlib.md5(file_desc.package + file_desc.name)
    pp_cpp.begin('void EnsureStructAliasSize_%s()' % m.hexdigest())

    for t, at in context.type_alias_messages.iteritems():
        pp_cpp.p('DM_STATIC_ASSERT(sizeof(%s) == sizeof(%s), Invalid_Struct_Alias_Size);' % (dot_to_cxx_namespace(t), at))

    pp_cpp.end()

def compile_cxx(context, proto_file, file_to_generate, namespace, includes):
    base_name = os.path.basename(file_to_generate)

    if base_name.rfind(".") != -1:
        base_name = base_name[0:base_name.rfind(".")]

    file_desc = proto_file

    file_h = context.response.file.add()
    file_h.name = base_name + ".h"

    f_h = StringIO()
    pp_h = PrettyPrinter(f_h, 0)

    guard_name = base_name.upper() + "_H"
    pp_h.p('#ifndef %s', guard_name)
    pp_h.p('#define %s', guard_name)
    pp_h.p("")

    pp_h.p('#include <stdint.h>')
    pp_h.p('#include <assert.h>')
    for d in file_desc.dependency:
        if not 'ddf_extensions' in d:
            pp_h.p('#include "%s"', d.replace(".proto", ".h"))
    pp_h.p('#include <dlib/align.h>')

    for i in includes:
        pp_h.p('#include "%s"', i)
    pp_h.p("")

    if namespace:
        pp_h.begin("namespace %s",  namespace)
    pp_h.begin("namespace %s",  file_desc.package)

    for mt in file_desc.enum_type:
        to_cxx_enum(context, pp_h, mt)

    for mt in file_desc.message_type:
        to_cxx_struct(context, pp_h, mt)

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

    for mt in file_desc.enum_type:
        to_cxx_enum_descriptor(context, pp_cpp, pp_h, mt, [file_desc.package])

    for mt in file_desc.message_type:
        to_cxx_descriptor(context, pp_cpp, pp_h, mt, [file_desc.package])

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
        self.type_name_to_java_type = {}
        self.type_alias_messages = {}
        self.response = CodeGeneratorResponse()

    # TODO: We add enum types as message types. Kind of hack...
    def add_message_type(self, package, java_package, java_outer_classname, message_type):
        if message_type.name in self.message_types:
            return
        n = str(package + '.' + message_type.name)
        self.message_types[n] = message_type

        if self.has_type_alias(n):
            self.type_alias_messages[n] = self.type_alias_name(n)

        self.type_name_to_java_type[package[1:] + '.' + message_type.name] = java_package + '.' + java_outer_classname + '.' + message_type.name

        if hasattr(message_type, 'nested_type'):
            for mt in message_type.nested_type:
                # TODO: add something to java_package here?
                self.add_message_type(package + '.' + message_type.name, java_package, java_outer_classname, mt)

            for et in message_type.enum_type:
                self.add_message_type(package + '.' + message_type.name, java_package, java_outer_classname, et)

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
    request.ParseFromString(sys.stdin.read())
    context = CompilerContext(request)

    for pf in request.proto_file:
        java_package = ''
        for x in pf.options.ListFields():
            if x[0].name == 'ddf_java_package':
                java_package = x[1]

        for mt in pf.message_type:
            context.add_message_type('.' + pf.package, java_package, pf.options.java_outer_classname, mt)

        for et in pf.enum_type:
            # NOTE: We add enum types as message types. Kind of hack...
            context.add_message_type('.' + pf.package, java_package, pf.options.java_outer_classname, et)

    for pf in request.proto_file:
        if pf.name == request.file_to_generate[0]:
            namespace = None
            for x in pf.options.ListFields():
                if x[0].name == 'ddf_namespace':
                    namespace = x[1]

            includes = []
            for x in pf.options.ListFields():
                if x[0].name == 'ddf_includes':
                    includes = x[1].split()

            if options.cxx:
                compile_cxx(context, pf, request.file_to_generate[0], namespace, includes)

            for x in pf.options.ListFields():
                if x[0].name == 'ddf_java_package':
                    if options.java:
                        compile_java(context, pf, x[1], request.file_to_generate[0])

    sys.stdout.write(context.response.SerializeToString())
