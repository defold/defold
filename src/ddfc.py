#!/usr/bin/python

from optparse import OptionParser

import sys, os
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
                  #FieldDescriptor.TYPE_BOOL : "bool",
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

type_to_size = { FieldDescriptor.TYPE_DOUBLE : 8,
                 FieldDescriptor.TYPE_FLOAT : 4,
                 FieldDescriptor.TYPE_INT64 : 8,
                 FieldDescriptor.TYPE_UINT64 : 8,
                 FieldDescriptor.TYPE_INT32 : 4,
#                  FieldDescriptor.TYPE_FIXED64
#                  FieldDescriptor.TYPE_FIXED32
                  #FieldDescriptor.TYPE_BOOL : "bool",
                 FieldDescriptor.TYPE_STRING : DDF_POINTER_SIZE,
#                  FieldDescriptor.TYPE_GROUP
#                  FieldDescriptor.TYPE_MESSAGE
#                  FieldDescriptor.TYPE_BYTES
                 FieldDescriptor.TYPE_UINT32 : 4,
                  FieldDescriptor.TYPE_ENUM : 4,
#                  FieldDescriptor.TYPE_SFIXED32
#                  FieldDescriptor.TYPE_SFIXED64
                 FieldDescriptor.TYPE_SINT32 : 4,
                 FieldDescriptor.TYPE_SINT64 : 8 }

class PrettyPrinter(object):
    def __init__(self, stream, initial_indent = 0):
        self.Stream = stream
        self.Indent = initial_indent

    def Begin(self, str, *format):
        str = str % format
        print >>self.Stream, " " * self.Indent + str
        print >>self.Stream, " " * self.Indent + "{"
        self.Indent += 4

    def BeginParen(self, str, *format):
        str = str % format
        print >>self.Stream, " " * self.Indent + str
        print >>self.Stream, " " * self.Indent + "("
        self.Indent += 4

    def Print(self, str, *format):
        str = str % format
        print >>self.Stream, " " * self.Indent + str

    def End(self, str = "", *format):
        str = str % format
        self.Indent -= 4
        print >>self.Stream, " " * self.Indent + "}%s;" % str
        print >>self.Stream, ""

    def EndParen(self, str = "", *format):
        str = str % format
        self.Indent -= 4
        print >>self.Stream, " " * self.Indent + ")%s;" % str
        print >>self.Stream, ""

def DotToNamespace(str):
    if str.startswith("."):
        str = str[1:]
    return str.replace(".", "::")

def ToCStruct(context, pp, message_type):
    # Calculate maximum lenght of "type"
    max_len = 0
    for f in message_type.field:
        if f.label == FieldDescriptor.LABEL_REPEATED:
            pass
        elif f.type  == FieldDescriptor.TYPE_BYTES:
            pass
        elif f.type == FieldDescriptor.TYPE_ENUM or f.type == FieldDescriptor.TYPE_MESSAGE:
            max_len = max(len(context.GetFieldTypeName(f)), max_len)
        else:
            max_len = max(len(type_to_ctype[f.type]), max_len)

    def p(t, n):
        pp.Print("%s%sm_%s;", t, (max_len-len(t) + 1) * " ",n)

    pp.Begin("struct %s", message_type.name)

    for et in message_type.enum_type:
        ToCEnum(context, pp, et)

    for nt in message_type.nested_type:
        ToCStruct(context, pp, nt)

    for f in message_type.field:
        if f.label == FieldDescriptor.LABEL_REPEATED or f.type == FieldDescriptor.TYPE_BYTES:
            pp.Begin("struct")
            if f.type ==  FieldDescriptor.TYPE_MESSAGE:
                type_name = DotToNamespace(f.type_name)
            elif f.type ==  FieldDescriptor.TYPE_BYTES:
                type_name = "uint8_t"
            else:
                type_name = type_to_ctype[f.type]

            pp.Print(type_name+"* m_Data;")
            if f.type == FieldDescriptor.TYPE_STRING:
                pp.Print("%s operator[](uint32_t i) const { assert(i < m_Count); return m_Data[i]; }", type_name)
            else:
                pp.Print("const %s& operator[](uint32_t i) const { assert(i < m_Count); return m_Data[i]; }", type_name)
                pp.Print("%s& operator[](uint32_t i) { assert(i < m_Count); return m_Data[i]; }", type_name)

            pp.Print("uint32_t " + "m_Count;")
            pp.End(" m_%s", f.name)
        elif f.type ==  FieldDescriptor.TYPE_ENUM or f.type == FieldDescriptor.TYPE_MESSAGE:
            p(context.GetFieldTypeName(f), f.name)
        else:
            p(type_to_ctype[f.type], f.name)
    pp.Print('')
    pp.Print('static dmDDF::Descriptor* m_DDFDescriptor;')
    pp.Print('static const uint64_t m_DDFHash;')
    pp.End()

def ToCEnum(context, pp, message_type):
    lst = []
    for f in message_type.value:
        lst.append((f.name, f.number))

    max_len = reduce(lambda y,x: max(len(x[0]), y), lst, 0)
    pp.Begin("enum %s",  message_type.name)

    for t,n in lst:
        pp.Print("%s%s= %d,", t, (max_len-len(t) + 1) * " ",n)

    pp.End()

def ToScriptName(name):
    script_name = ""
    for i in range(len(name)):
        if i > 0 and (name[i-1:i].islower() or name[i-1:i].isdigit() or (i+1 < len(name) and name[i+1:i+2].islower())) and name[i:i+1].isupper():
            script_name += "_"
        script_name += name[i:i+1].lower()
    return script_name

def ToDescriptor(context, pp_cpp, pp_h, message_type, namespace_lst):
    namespace = "_".join(namespace_lst)

    pp_h.Print('extern dmDDF::Descriptor %s_%s_DESCRIPTOR;', namespace, message_type.name)

    for nt in message_type.nested_type:
        ToDescriptor(context, pp_cpp, pp_h, nt, namespace_lst + [message_type.name] )

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

        tpl += ("DDF_OFFSET_OF(%s::%s, m_%s)" % (namespace.replace("_", "::"), message_type.name, f.name), )

        lst.append(tpl)

    pp_cpp.Begin("dmDDF::FieldDescriptor %s_%s_FIELDS_DESCRIPTOR[] = ", namespace, message_type.name)

    for name, number, type, label, msg_desc, offset in lst:
        pp_cpp.Print('{ "%s", "%s", %d, %d, %d, %s, %s },'  % (name, ToScriptName(name), number, type, label, msg_desc, offset))

    pp_cpp.End()

    pp_cpp.Begin("dmDDF::Descriptor %s_%s_DESCRIPTOR = ", namespace, message_type.name)
    pp_cpp.Print('%d, %d,', DDF_MAJOR_VERSION, DDF_MINOR_VERSION)
    pp_cpp.Print('"%s",', message_type.name)
    pp_cpp.Print('"%s",', ToScriptName(message_type.name))
    pp_cpp.Print('sizeof(%s::%s),', namespace.replace("_", "::"), message_type.name)
    pp_cpp.Print('%s_%s_FIELDS_DESCRIPTOR,', namespace, message_type.name)
    pp_cpp.Print('sizeof(%s_%s_FIELDS_DESCRIPTOR)/sizeof(dmDDF::FieldDescriptor),', namespace, message_type.name)
    pp_cpp.End()

    pp_cpp.Print('dmDDF::Descriptor* %s::%s::m_DDFDescriptor = &%s_%s_DESCRIPTOR;' % ('::'.join(namespace_lst), message_type.name, namespace, message_type.name))

    # TODO: This is not optimal. Hash value is sensitive on googles format string
    # Also dependent on type invariant values?
    hash_string = str(message_type).replace(" ", "").replace("\n", "").replace("\r", "")
    pp_cpp.Print('const uint64_t %s::%s::m_DDFHash = 0x%016XLL;' % ('::'.join(namespace_lst), message_type.name, dlib.dmHashBuffer64(hash_string)))

    pp_cpp.Print('')

def ToEnumDescriptor(context, pp_cpp, pp_h, enum_type, namespace_lst):
    namespace = "_".join(namespace_lst)

    pp_h.Print("extern dmDDF::EnumDescriptor %s_%s_DESCRIPTOR;", namespace, enum_type.name)

    lst = []
    for f in enum_type.value:
        tpl = (f.name, f.number)
        lst.append(tpl)

    pp_cpp.Begin("dmDDF::EnumValueDescriptor %s_%s_FIELDS_DESCRIPTOR[] = ", namespace, enum_type.name)

    for name, number in lst:
        pp_cpp.Print('{ "%s", %d },'  % (name, number))

    pp_cpp.End()

    pp_cpp.Begin("dmDDF::EnumDescriptor %s_%s_DESCRIPTOR = ", namespace, enum_type.name)
    pp_cpp.Print('%d, %d,', DDF_MAJOR_VERSION, DDF_MINOR_VERSION)
    pp_cpp.Print('"%s",', enum_type.name)
    pp_cpp.Print('%s_%s_FIELDS_DESCRIPTOR,', namespace, enum_type.name)
    pp_cpp.Print('sizeof(%s_%s_FIELDS_DESCRIPTOR)/sizeof(dmDDF::EnumValueDescriptor),', namespace, enum_type.name)
    pp_cpp.End()

def DotToJavaPackage(context, str, proto_package, java_package):
    if str.startswith("."):
        str = str[1:]

    ret = context.TypeNameToJavaType[str]
    ret = ret.replace(java_package + '.', "")
    return ret

def ToJavaEnum(context, pp, message_type):
    lst = []
    for f in message_type.value:
        lst.append(("public static final int %s_%s" % (message_type.name, f.name), f.number))

    max_len = reduce(lambda y,x: max(len(x[0]), y), lst, 0)
    #pp.Begin("enum %s",  message_type.name)

    for t,n in lst:
        pp.Print("%s%s= %d;", t, (max_len-len(t) + 1) * " ",n)
    pp.Print("")
    #pp.End()

def ToJavaEnumDescriptor(context, pp, enum_type, qualified_proto_package):

    lst = []
    for f in enum_type.value:
        tpl = (f.name, f.number)
        lst.append(tpl)

    pp.Begin('public static class %s' % enum_type.name)
    pp.Begin("public static com.dynamo.ddf.EnumValueDescriptor VALUES_DESCRIPTOR[] = new com.dynamo.ddf.EnumValueDescriptor[]")

    for name, number in lst:
        pp.Print('new com.dynamo.ddf.EnumValueDescriptor("%s", %d),'  % (name, number))

    pp.End()
    name = qualified_proto_package.replace('$', '_').replace('.', '_')
    pp.Print('public static com.dynamo.ddf.EnumDescriptor DESCRIPTOR = new com.dynamo.ddf.EnumDescriptor("%s", VALUES_DESCRIPTOR);' % (name))
    pp.End()

def ToJavaDescriptor(context, pp, message_type, proto_package, java_package, qualified_proto_package):

    lst = []
    for f in message_type.field:
        tpl = (f.name, f.number, f.type, f.label)
        if f.type ==  FieldDescriptor.TYPE_MESSAGE:
            tmp = DotToJavaPackage(context, f.type_name, proto_package, java_package)
            if tmp.startswith("."):
                tmp = tmp[1:]
            tpl += (tmp + ".DESCRIPTOR", "null")
        elif f.type ==  FieldDescriptor.TYPE_ENUM:
            tmp = DotToJavaPackage(context, f.type_name, proto_package, java_package)
            if tmp.startswith("."):
                tmp = tmp[1:]
            tpl += ("null", tmp + ".DESCRIPTOR", )

        else:
            tpl += ("null", "null")
        lst.append(tpl)

    pp.Begin("public static com.dynamo.ddf.FieldDescriptor FIELDS_DESCRIPTOR[] = new com.dynamo.ddf.FieldDescriptor[]")

    for name, number, type, label, msg_desc, enum_desc in lst:
        pp.Print('new com.dynamo.ddf.FieldDescriptor("%s", %d, %d, %d, %s, %s),'  % (name, number, type, label, msg_desc, enum_desc))

    pp.End()

    pp.BeginParen("public static com.dynamo.ddf.Descriptor DESCRIPTOR = new com.dynamo.ddf.Descriptor")
    pp.Print('"%s",', qualified_proto_package.replace('$', '_').replace('.', '_'))
    pp.Print('FIELDS_DESCRIPTOR')
    pp.EndParen('')

    # TODO: Add hash support for java types?
    # TODO: This is not optimal. Hash value is sensitive on googles format string
    # Also dependent on type invariant values?
    hash_string = str(message_type).replace(" ", "").replace("\n", "").replace("\r", "")
    #pp.Print('const uint64_t %s::%s::m_DDFHash = 0x%016XLL;' % ('::'.join(namespace_lst), message_type.name, dlib.dmHashBuffer64(hash_string)))

def ToJavaClass(context, pp, message_type, proto_package, java_package, qualified_proto_package):
    # Calculate maximum length of "type"
    max_len = 0
    for f in message_type.field:
        if f.label == FieldDescriptor.LABEL_REPEATED:
            if f.type ==  FieldDescriptor.TYPE_MESSAGE:
                tn = DotToJavaPackage(context, f.type_name, proto_package, java_package)
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
            max_len = max(len(DotToJavaPackage(context, f.type_name, proto_package, java_package)), max_len)
        else:
            max_len = max(len(type_to_javatype[f.type]), max_len)

    def p(t, n, assign = None):
        if assign:
            pp.Print("public %s%sm_%s = %s;", t, (max_len-len(t) + 1) * " ", n, assign)
        else:
            pp.Print("public %s%sm_%s;", t, (max_len-len(t) + 1) * " ", n)

    pp.Print('@ProtoClassName(name = "%s")' % qualified_proto_package)
    pp.Begin("public static final class %s", message_type.name)

    for et in message_type.enum_type:
        ToJavaEnumDescriptor(context, pp, et, qualified_proto_package + '_' + et.name)
        ToJavaEnum(context, pp, et)

    ToJavaDescriptor(context, pp, message_type, proto_package, java_package, qualified_proto_package)

    for nt in message_type.nested_type:
        ToJavaClass(context, pp, nt, proto_package, java_package, qualified_proto_package + "$" + nt.name)

    for f in message_type.field:
        if f.label == FieldDescriptor.LABEL_REPEATED:
            if f.type ==  FieldDescriptor.TYPE_MESSAGE:
                type_name = DotToJavaPackage(context, f.type_name, proto_package, java_package)
            elif f.type ==  FieldDescriptor.TYPE_BYTES:
                type_name = "Byte"
            else:
                type_name = type_to_boxedjavatype[f.type]

            pp.Print('@ComponentType(type = %s.class)' % type_name)
            p('List<%s>' % (type_name), f.name, 'new ArrayList<%s>()' % type_name)
        elif f.type == FieldDescriptor.TYPE_BYTES:
            p('byte[]', f.name)
        elif f.type == FieldDescriptor.TYPE_ENUM:
            p("int", f.name)
        elif f.type == FieldDescriptor.TYPE_MESSAGE:
            java_type_name = DotToJavaPackage(context, f.type_name, proto_package, java_package)
            p(java_type_name, f.name, 'new %s()' % java_type_name)
        else:
            p(type_to_javatype[f.type], f.name)
    pp.End()

def CompileJava(context, proto_file, ddf_java_package, file_to_generate):
    file_desc = proto_file

    path = ddf_java_package.replace('.', '/')


    file_java = context.Response.file.add()
    file_java.name = os.path.join(path, proto_file.options.java_outer_classname + '.java')
    f_java = StringIO()

    pp_java = PrettyPrinter(f_java, 0)

    if ddf_java_package != '':
        pp_java.Print("package %s;",  ddf_java_package)
    pp_java.Print("")
    pp_java.Print("import java.util.List;")
    pp_java.Print("import java.util.ArrayList;")
    pp_java.Print("import com.dynamo.ddf.annotations.ComponentType;")
    pp_java.Print("import com.dynamo.ddf.annotations.ProtoClassName;")

    pp_java.Print("")
    pp_java.Begin("public final class %s", proto_file.options.java_outer_classname)

    for mt in file_desc.enum_type:
        ToJavaEnumDescriptor(context, pp_java, mt, ddf_java_package + '_' + et.name)
        ToJavaEnum(context, pp_java, mt)

    for mt in file_desc.message_type:
        ToJavaClass(context, pp_java, mt, file_desc.package,
                    ddf_java_package + "." + proto_file.options.java_outer_classname,
                    file_desc.options.java_package + "." + file_desc.options.java_outer_classname + "$" + mt.name)

    pp_java.End("")

    file_java.content = f_java.getvalue()

def ToEnsureStructAliasSize(context, file_desc, pp_cpp):
    import hashlib
    m = hashlib.md5(file_desc.package + file_desc.name)
    pp_cpp.Begin('void EnsureStructAliasSize_%s()' % m.hexdigest())

    for t, at in context.TypeAliasMessages.iteritems():
        pp_cpp.Print('DDF_STATIC_ASSERT(sizeof(%s) == sizeof(%s), Invalid_Struct_Alias_Size);' % (DotToNamespace(t), at))

    pp_cpp.End()

def Compile(context, proto_file, file_to_generate, namespace, includes):
    base_name = os.path.basename(file_to_generate)

    if base_name.rfind(".") != -1:
        base_name = base_name[0:base_name.rfind(".")]

    file_desc = proto_file

    file_h = context.Response.file.add()
    file_h.name = base_name + ".h"

    f_h = StringIO()
    pp_h = PrettyPrinter(f_h, 0)

    guard_name = base_name.upper() + "_H"
    pp_h.Print('#ifndef %s', guard_name)
    pp_h.Print('#define %s', guard_name)
    pp_h.Print("")

    pp_h.Print('#include <stdint.h>')
    pp_h.Print('#include <assert.h>')
    for d in file_desc.dependency:
        if not 'ddf_extensions' in d:
            pp_h.Print('#include "%s"', d.replace(".proto", ".h"))

    for i in includes:
        pp_h.Print('#include "%s"', i)
    pp_h.Print("")

    if namespace:
        pp_h.Begin("namespace %s",  namespace)
    pp_h.Begin("namespace %s",  file_desc.package)

    for mt in file_desc.enum_type:
        ToCEnum(context, pp_h, mt)

    for mt in file_desc.message_type:
        ToCStruct(context, pp_h, mt)

    pp_h.End()

    file_cpp = context.Response.file.add()
    file_cpp.name = base_name + ".cpp"
    f_cpp = StringIO()

    pp_cpp = PrettyPrinter(f_cpp, 0)
    pp_cpp.Print('#include <ddf/ddf.h>')
    for d in file_desc.dependency:
        if not 'ddf_extensions' in d:
            pp_cpp.Print('#include "%s"', d.replace(".proto", ".h"))
    pp_cpp.Print('#include "%s.h"' % base_name)

    if namespace:
        pp_cpp.Begin("namespace %s",  namespace)

    pp_h.Print("#ifdef DDF_EXPOSE_DESCRIPTORS")

    for mt in file_desc.enum_type:
        ToEnumDescriptor(context, pp_cpp, pp_h, mt, [file_desc.package])

    for mt in file_desc.message_type:
        ToDescriptor(context, pp_cpp, pp_h, mt, [file_desc.package])

    pp_h.Print("#endif")

    if namespace:
        pp_h.End()

    ToEnsureStructAliasSize(context, file_desc, pp_cpp)

    if namespace:
        pp_cpp.End()

    file_cpp.content = f_cpp.getvalue()

    pp_h.Print('#endif // %s ', guard_name)
    file_h.content = f_h.getvalue()


class CompilerContext(object):
    def __init__(self, request):
        self.Request = request
        self.MessageTypes = {}
        self.TypeNameToJavaType = {}
        self.TypeAliasMessages = {}
        self.Response = CodeGeneratorResponse()

    # TODO: We add enum types as message types. Kind of hack...
    def AddMessageType(self, package, java_package, java_outer_classname, message_type):
        if message_type.name in self.MessageTypes:
            return
        n = str(package + '.' + message_type.name)
        self.MessageTypes[n] = message_type

        if self.HasTypeAlias(n):
            self.TypeAliasMessages[n] = self.TypeAliasName(n)

        self.TypeNameToJavaType[package[1:] + '.' + message_type.name] = java_package + '.' + java_outer_classname + '.' + message_type.name

        if hasattr(message_type, 'nested_type'):
            for mt in message_type.nested_type:
                # TODO: add something to java_package here?
                self.AddMessageType(package + '.' + message_type.name, java_package, java_outer_classname, mt)

            for et in message_type.enum_type:
                self.AddMessageType(package + '.' + message_type.name, java_package, java_outer_classname, et)

    def HasTypeAlias(self, type_name):
        mt = self.MessageTypes[type_name]
        for x in mt.options.ListFields():
            if x[0].name == 'alias':
                return True
        return False

    def TypeAliasName(self, type_name):
        mt = self.MessageTypes[type_name]
        for x in mt.options.ListFields():
            if x[0].name == 'alias':
                return x[1]
        assert False

    def GetFieldTypeName(self, f):
        if f.type ==  FieldDescriptor.TYPE_MESSAGE:
            if self.HasTypeAlias(f.type_name):
                return self.TypeAliasName(f.type_name)
            else:
                return DotToNamespace(f.type_name)
        elif f.type == FieldDescriptor.TYPE_ENUM:
            return DotToNamespace(f.type_name)
        else:
            assert(False)

if __name__ == '__main__':

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
            context.AddMessageType('.' + pf.package, java_package, pf.options.java_outer_classname, mt)

        for et in pf.enum_type:
            # NOTE: We add enum types as message types. Kind of hack...
            context.AddMessageType('.' + pf.package, java_package, pf.options.java_outer_classname, et)

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
                Compile(context, pf, request.file_to_generate[0], namespace, includes)

            for x in pf.options.ListFields():
                if x[0].name == 'ddf_java_package':
                    if options.java:
                        CompileJava(context, pf, x[1], request.file_to_generate[0])

    sys.stdout.write(context.Response.SerializeToString())
