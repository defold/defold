from optparse import OptionParser

import sys, os

from google.protobuf.descriptor import FieldDescriptor
from google.protobuf.descriptor_pb2 import FileDescriptorSet
from google.protobuf.descriptor_pb2 import FieldDescriptorProto

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
        elif f.type ==  FieldDescriptor.TYPE_ENUM:
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
        if f.label == FieldDescriptor.LABEL_REPEATED:
            pp.Begin("struct")
            if f.type ==  FieldDescriptor.TYPE_MESSAGE:
                pp.Print(DotToNamespace(f.type_name)+"* m_Data;")
            else:
                pp.Print(type_to_ctype[f.type]+"* m_Data;")
            pp.Print("uint32_t " + "m_Count;")
            pp.End(" m_%s", f.name)
        elif f.type ==  FieldDescriptor.TYPE_ENUM:
            p(DotToNamespace(f.type_name), f.name)
        else:
            p(type_to_ctype[f.type], f.name)

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

def GetAlignment(message_type):
    align = 0
    for f in message_type.field:
        if f.label == FieldDescriptor.LABEL_REPEATED:
            align = max(align, DDF_POINTER_SIZE)
        elif f.type ==  FieldDescriptor.TYPE_MESSAGE:
            align = max(align, GetAlignment(f.message_type))
        else:
            align = max(align, type_to_size[f.type])
            
    return align

def ToDescriptor(pp_cpp, pp_h, message_type, namespace_lst):
    namespace = "_".join(namespace_lst)

    pp_h.Print("extern SDDFDescriptor %s_%s_DESCRIPTOR;", namespace, message_type.name)

    for nt in message_type.nested_type:
        ToDescriptor(pp_cpp, nt, namespace_lst + [message_type.name] )
    
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
    
    pp_cpp.Begin("SDDFFieldDescriptor %s_%s_FIELDS_DESCRIPTOR[] = ", namespace, message_type.name)

    for name, number, type, label, msg_desc, offset in lst:
        pp_cpp.Print('{ "%s", %d, %d, %d, %s, %s },'  % (name, number, type, label, msg_desc, offset))
        
    pp_cpp.End()

    pp_cpp.Begin("SDDFDescriptor %s_%s_DESCRIPTOR = ", namespace, message_type.name)
    pp_cpp.Print('"%s",', message_type.name)
    pp_cpp.Print('sizeof(%s::%s),', namespace.replace("-", "::"), message_type.name)
    pp_cpp.Print('%d,', GetAlignment(message_type))
    pp_cpp.Print('%s_%s_FIELDS_DESCRIPTOR,', namespace, message_type.name)
    pp_cpp.Print('sizeof(%s_%s_FIELDS_DESCRIPTOR)/sizeof(SDDFFieldDescriptor),', namespace, message_type.name)
    pp_cpp.End()

def Compile(input_file, output_dir, namespace):
    base_name = os.path.basename(input_file)

    if base_name.rfind(".") != -1:
        base_name = base_name[0:base_name.rfind(".")]

    fds = FileDescriptorSet()
    fds.ParseFromString(open(input_file, "rb").read())

    stream = sys.stdout
    file_desc = fds.file[0]

    f_h = open(os.path.join(output_dir, base_name + ".h"), "w")
    pp_h = PrettyPrinter(f_h, 0)
        
    guard_name = base_name.upper() + "_H"
    pp_h.Print('#ifndef %s', guard_name)
    pp_h.Print('#define %s', guard_name)
    pp_h.Print("")

    pp_h.Print('#include <stdint.h>')
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
    pp_cpp.Print('#include "ddf.h"')
    for d in file_desc.dependency:
        pp_cpp.Print('#include "%s"', d.replace(".proto", ".h"))
    pp_cpp.Print('#include "%s.h"' % base_name)

    if namespace:
        pp_cpp.Begin("namespace %s",  namespace)

    pp_h.Print("#ifdef DDF_EXPOSE_DESCRIPTORS")

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
    (options, args) = parser.parse_args()

    if not options.output_file:
        parser.error("Output file not (-o) not specified")

    if len(args) == 0:
        parser.error("No input file specified")

    output_dir = os.path.dirname(options.output_file)
    Compile(args[0], output_dir, options.namespace)
