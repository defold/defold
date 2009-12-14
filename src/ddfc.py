from optparse import OptionParser

import sys, os
import dlib

from google.protobuf.descriptor import FieldDescriptor
from google.protobuf.descriptor_pb2 import FileDescriptorSet
from google.protobuf.descriptor_pb2 import FieldDescriptorProto

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

    def Print(self, str, *format):
        str = str % format
        print >>self.Stream, " " * self.Indent + str

    def End(self, str = "", *format):
        str = str % format
        self.Indent -= 4
        print >>self.Stream, " " * self.Indent + "}%s;" % str
        print >>self.Stream, ""

def DotToNamespace(str):
    if str.startswith("."):
        str = str[1:]
    return str.replace(".", "::")

def ToCStruct(pp, message_type):
    # Calculate maximum lenght of "type"
    max_len = 0
    for f in message_type.field:
        if f.label == FieldDescriptor.LABEL_REPEATED:
            pass
        elif f.type  == FieldDescriptor.TYPE_BYTES:
            pass
        elif f.type == FieldDescriptor.TYPE_ENUM or f.type == FieldDescriptor.TYPE_MESSAGE:
            max_len = max(len(DotToNamespace(f.type_name)), max_len)
        else:
            max_len = max(len(type_to_ctype[f.type]), max_len)

    def p(t, n):
        pp.Print("%s%sm_%s;", t, (max_len-len(t) + 1) * " ",n)

    pp.Begin("struct %s", message_type.name)

    for et in message_type.enum_type:
        ToCEnum(pp, et)

    for nt in message_type.nested_type:
        ToCStruct(pp, nt)

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
            p(DotToNamespace(f.type_name), f.name)
        else:
            p(type_to_ctype[f.type], f.name)
    pp.Print('')
    pp.Print('static dmDDF::Descriptor* m_DDFDescriptor;')
    pp.Print('static const uint64_t m_DDFHash;')
    pp.End()

def ToCEnum(pp, message_type):
    lst = []
    for f in message_type.value:
        lst.append((f.name, f.number))

    max_len = reduce(lambda y,x: max(len(x[0]), y), lst, 0)
    pp.Begin("enum %s",  message_type.name)

    for t,n in lst:
        pp.Print("%s%s= %d,", t, (max_len-len(t) + 1) * " ",n)

    pp.End()

def ToDescriptor(pp_cpp, pp_h, message_type, namespace_lst):
    namespace = "_".join(namespace_lst)

    pp_h.Print('extern dmDDF::Descriptor %s_%s_DESCRIPTOR;', namespace, message_type.name)

    for nt in message_type.nested_type:
        ToDescriptor(pp_cpp, pp_h, nt, namespace_lst + [message_type.name] )

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
        pp_cpp.Print('{ "%s", %d, %d, %d, %s, %s },'  % (name, number, type, label, msg_desc, offset))

    pp_cpp.End()

    pp_cpp.Begin("dmDDF::Descriptor %s_%s_DESCRIPTOR = ", namespace, message_type.name)
    pp_cpp.Print('%d, %d,', DDF_MAJOR_VERSION, DDF_MINOR_VERSION)
    pp_cpp.Print('"%s",', message_type.name)
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

def ToEnumDescriptor(pp_cpp, pp_h, enum_type, namespace_lst):
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

def DotToJavaPackage(str, proto_package, java_package):
    if str.startswith("."):
        str = str[1:]
    tmp = str.replace(proto_package, java_package)
    if True:
        # Strip qualifying package name from classes in "this" package
        # TODO: This should work with referring types too?
        return tmp.replace(java_package + '.', "")
    else:
        return tmp

def ToJavaEnum(pp, message_type):
    lst = []
    for f in message_type.value:
        lst.append(("public static final int %s_%s" % (message_type.name, f.name), f.number))

    max_len = reduce(lambda y,x: max(len(x[0]), y), lst, 0)
    #pp.Begin("enum %s",  message_type.name)

    for t,n in lst:
        pp.Print("%s%s= %d;", t, (max_len-len(t) + 1) * " ",n)
    pp.Print("")
    #pp.End()

def ToJavaClass(pp, message_type, proto_package, java_package, qualified_proto_package):
    # Calculate maximum lenght of "type"
    max_len = 0
    for f in message_type.field:
        if f.label == FieldDescriptor.LABEL_REPEATED:
            if f.type ==  FieldDescriptor.TYPE_MESSAGE:
                tn = DotToJavaPackage(f.type_name, proto_package, java_package)
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
            max_len = max(len(DotToJavaPackage(f.type_name, proto_package, java_package)), max_len)
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
        ToJavaEnum(pp, et)

    for nt in message_type.nested_type:
        ToJavaClass(pp, nt, proto_package, java_package, qualified_proto_package + "$" + nt.name)

    for f in message_type.field:
        if f.label == FieldDescriptor.LABEL_REPEATED:
            if f.type ==  FieldDescriptor.TYPE_MESSAGE:
                type_name = DotToJavaPackage(f.type_name, proto_package, java_package)
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
            java_type_name = DotToJavaPackage(f.type_name, proto_package, java_package)
            p(java_type_name, f.name, 'new %s()' % java_type_name)
        else:
            p(type_to_javatype[f.type], f.name)
#    pp.Print('')
#    pp.Print('static SDDFDescriptor* m_DDFDescriptor;')
#    pp.Print('static const uint64_t m_DDFHash;')
    pp.End()

def CompileJava(input_file, options):
    fds = FileDescriptorSet()
    fds.ParseFromString(open(input_file, "rb").read())

    file_desc = fds.file[0]

    path = os.path.join(options.java_out, options.java_package.replace('.', '/'))
    if not os.path.exists(path):
        os.makedirs(path)

    f_java = open(os.path.join(path, options.java_classname + '.java'), 'w')
    pp_java = PrettyPrinter(f_java, 0)

    if options.java_package != '':
        pp_java.Print("package %s;",  options.java_package)
    pp_java.Print("")
    pp_java.Print("import java.util.List;")
    pp_java.Print("import java.util.ArrayList;")
    pp_java.Print("import com.dynamo.ddf.annotations.ComponentType;")
    pp_java.Print("import com.dynamo.ddf.annotations.ProtoClassName;")

    pp_java.Print("")
    pp_java.Begin("public final class %s", options.java_classname)

    for mt in file_desc.enum_type:
        ToJavaEnum(pp_java, mt)

    for mt in file_desc.message_type:
        ToJavaClass(pp_java, mt, file_desc.package,
                    options.java_package + "." + options.java_classname,
                    file_desc.options.java_package + "." + file_desc.options.java_outer_classname + "$" + mt.name)

    pp_java.End("")

def Compile(input_file, output_dir, options):
    namespace = options.namespace
    base_name = os.path.basename(input_file)

    if base_name.rfind(".") != -1:
        base_name = base_name[0:base_name.rfind(".")]

    fds = FileDescriptorSet()
    fds.ParseFromString(open(input_file, "rb").read())

    file_desc = fds.file[0]

    f_h = open(os.path.join(output_dir, base_name + ".h"), "w")
    pp_h = PrettyPrinter(f_h, 0)

    guard_name = base_name.upper() + "_H"
    pp_h.Print('#ifndef %s', guard_name)
    pp_h.Print('#define %s', guard_name)
    pp_h.Print("")

    pp_h.Print('#include <stdint.h>')
    pp_h.Print('#include <assert.h>')
    for d in file_desc.dependency:
        pp_h.Print('#include "%s"', d.replace(".proto", ".h"))
    pp_h.Print("")

    if namespace:
        pp_h.Begin("namespace %s",  namespace)
    pp_h.Begin("namespace %s",  file_desc.package)

    for mt in file_desc.enum_type:
        ToCEnum(pp_h, mt)

    for mt in file_desc.message_type:
        ToCStruct(pp_h, mt)

    pp_h.End()

    f_cpp = open(os.path.join(output_dir, base_name + ".cpp"), "w")
    pp_cpp = PrettyPrinter(f_cpp, 0)
    pp_cpp.Print('#include <ddf/ddf.h>')
    for d in file_desc.dependency:
        pp_cpp.Print('#include "%s"', d.replace(".proto", ".h"))
    pp_cpp.Print('#include "%s.h"' % base_name)

    if namespace:
        pp_cpp.Begin("namespace %s",  namespace)

    pp_h.Print("#ifdef DDF_EXPOSE_DESCRIPTORS")

    for mt in file_desc.enum_type:
        ToEnumDescriptor(pp_cpp, pp_h, mt, [file_desc.package])

    for mt in file_desc.message_type:
        ToDescriptor(pp_cpp, pp_h, mt, [file_desc.package])

    pp_h.Print("#endif")

    if namespace:
        pp_h.End()

    if namespace:
        pp_cpp.End()

    f_cpp.close()

    pp_h.Print('#endif // %s ', guard_name)
    f_h.close()

if __name__ == '__main__':
    usage = "usage: %prog [options] FILE"
    parser = OptionParser(usage = usage)
    parser.add_option("-o", dest="output_file", help="Output file (.cpp)", metavar="FILE")
    parser.add_option("--ns", dest="namespace", help="Namespace", metavar="NAMESPACE")
    parser.add_option("--java_package", default=None, dest="java_package", help="Java packge name", metavar="JAVA_PACKAGE")
    parser.add_option("--java_classname", default=None, dest="java_classname", help="Java class name", metavar="JAVA_CLASSNAME")
    parser.add_option("--java_out", default=None, dest="java_out", help="Java output directory")
    (options, args) = parser.parse_args()

    if not options.output_file:
        parser.error("Output file not (-o) not specified")

    if len(args) == 0:
        parser.error("No input file specified")

    if options.java_out and options.java_package == None:
        parser.error("No java package specified")

    if options.java_out and options.java_classname == None:
        parser.error("No java class name specified")

    output_dir = os.path.dirname(options.output_file)
    Compile(args[0], output_dir, options)

    if options.java_out:
        CompileJava(args[0], options)
