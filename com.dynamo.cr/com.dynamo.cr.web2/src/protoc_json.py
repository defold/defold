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

type_to_javatype = { FieldDescriptor.TYPE_DOUBLE : "double",
                  FieldDescriptor.TYPE_FLOAT : "float",
                  FieldDescriptor.TYPE_INT64 : "long",
                  FieldDescriptor.TYPE_UINT64 : "long",
                  FieldDescriptor.TYPE_INT32 : "int",
#                  FieldDescriptor.TYPE_FIXED64
#                  FieldDescriptor.TYPE_FIXED32
                  #FieldDescriptor.TYPE_BOOL : "bool",
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
                  #FieldDescriptor.TYPE_BOOL : "bool",
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

    print >>sys.stderr, sys.argv
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
                print >>sys.stderr, x[0].name

                if x[0].name == 'ddf_java_package':
                    if options.java:
                        compile_java(context, pf, x[1], request.file_to_generate[0])

    sys.stdout.write(context.response.SerializeToString())
