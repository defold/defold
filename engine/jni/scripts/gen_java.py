#-------------------------------------------------------------------------------
#   Generate Java+Jni bindings from C++
#
#   C++ coding style:
#   ...
#   Notes
#   - functions are not supported
#-------------------------------------------------------------------------------

import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'external'))

import gen_ir
import os, sys

import gen_util as util

ignores = [
    # 'sdtx_printf',
    # ...
]

# NOTE: syntax for function results: "func_name.RESULT"
overrides = {
    # 'sgl_error':                            'sgl_get_error',   # 'error' is reserved in Zig
    # 'sgl_deg':                              'sgl_as_degrees',
    # 'sgl_rad':                              'sgl_as_radians',
    # 'sg_context_desc.color_format':         'int',
    # 'sg_context_desc.depth_format':         'int',
    # 'sg_apply_uniforms.ub_index':           'uint32_t',
    # 'sg_draw.base_element':                 'uint32_t',
    # 'sg_draw.num_elements':                 'uint32_t',
    # 'sg_draw.num_instances':                'uint32_t',
    # 'sshape_element_range_t.base_element':  'uint32_t',
    # 'sshape_element_range_t.num_elements':  'uint32_t',
    # 'sdtx_font.font_index':                 'uint32_t',
    # 'SGL_NO_ERROR':                         'SGL_ERROR_NO_ERROR',
}

prim_types = {
    'int':          'i32',
    'bool':         'bool',
    'char':         'u8',
    'int8_t':       'i8',
    'uint8_t':      'u8',
    'int16_t':      'i16',
    'uint16_t':     'u16',
    'int32_t':      'i32',
    'uint32_t':     'u32',
    'int64_t':      'i64',
    'uint64_t':     'u64',
    'float':        'f32',
    'double':       'f64',
    'uintptr_t':    'usize',
    'intptr_t':     'isize',
    'size_t':       'usize'
}

prim_defaults = {
    'int':          '0',
    'bool':         'false',
    'int8_t':       '0',
    'uint8_t':      '0',
    'int16_t':      '0',
    'uint16_t':     '0',
    'int32_t':      '0',
    'uint32_t':     '0',
    'int64_t':      '0',
    'uint64_t':     '0',
    'float':        '0.0',
    'double':       '0.0',
    'uintptr_t':    '0',
    'intptr_t':     '0',
    'size_t':       '0'
}

# https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/types.html
c_type_to_jni_type_specifiers = {
    'int':          'I',
    'bool':         'Z',
    'int8_t':       'C',
    'uint8_t':      'B',
    'int16_t':      'S',
    'uint16_t':     '0',
    'int32_t':      'I',
    'uint32_t':     'I',
    'int64_t':      'L',
    'uint64_t':     'L',
    'float':        'F',
    'double':       'D',
}

# https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/functions.html#Set_type_Field_routines
c_type_to_jni_capname = {
    'bool':     'Boolean',
    'char':     'Char',
    'int8_t':   'Char',
    'uint8_t':  'Byte',
    'int16_t':  'Short',
    'uint16_t': 'Short',
    'int':      'Int',
    'int32_t':  'Int',
    'uint32_t': 'Int',
    'int64_t':  'Long',
    'uint64_t': 'Long',
    'float':    'Float',
    'double':   'Double',
}

c_type_to_jni_type = {
    'bool':     'jboolean',
    'char':     'jchar',
    'int8_t':   'jchar',
    'uint8_t':  'jbyte',
    'int16_t':  'jshort',
    'uint16_t': 'jshort',
    'int':      'jint',
    'int32_t':  'jint',
    'uint32_t': 'jint',
    'int64_t':  'jlong',
    'uint64_t': 'jlong',
    'float':    'jfloat',
    'double':   'jdouble',
}

c_type_to_java_type = {
    'bool':     'boolean',
    'char':     'char',
    'int8_t':   'char',
    'uint8_t':  'byte',
    'int16_t':  'short',
    'uint16_t': 'short',
    'int':      'int',
    'int32_t':  'int',
    'uint32_t': 'int',
    'int64_t':  'long',
    'uint64_t': 'long',
    'float':    'float',
    'double':   'double',
}

struct_types = []
enum_types = []
enum_items = {}
out_lines = ''
struct_jni_types = []

def reset_lines():
    global out_lines
    global struct_jni_types
    out_lines = ''
    struct_jni_types = []

def reset_globals():
    global struct_types
    global enum_types
    global enum_items
    global struct_jni_types
    struct_types = []
    enum_types = []
    enum_items = {}
    struct_jni_types = []
    reset_lines()

def l(s):
    global out_lines
    out_lines += s + '\n'

def as_zig_prim_type(s):
    return prim_types[s]

# prefix_bla_blub(_t) => (dep.)BlaBlub
def as_zig_struct_type(s, prefix):
    parts = s.lower().split('_')
    outp = '' if s.startswith(prefix) else f'{parts[0]}.'
    for part in parts[1:]:
        # ignore '_t' type postfix
        if (part != 't'):
            outp += part.capitalize()
    return outp

# prefix_bla_blub(_t) => (dep.)BlaBlub
def as_zig_enum_type(s, prefix):
    parts = s.lower().split('_')
    outp = '' if s.startswith(prefix) else f'{parts[0]}.'
    for part in parts[1:]:
        if (part != 't'):
            outp += part.capitalize()
    return outp

def check_override(name, default=None):
    if name in overrides:
        return overrides[name]
    elif default is None:
        return name
    else:
        return default

def check_ignore(name):
    return name in ignores

# PREFIX_ENUM_BLA => Bla, _PREFIX_ENUM_BLA => Bla
def as_enum_item_name(s):
    outp = s.lstrip('_')
    parts = outp.split('_')[2:]
    if not parts:
        return s
    outp = '_'.join(parts)
    if outp[0].isdigit():
        outp = '_' + outp
    return outp

def enum_default_item(enum_name):
    return enum_items[enum_name][0]

def is_prim_type(s):
    return s in prim_types

def is_struct_type(s):
    return s in struct_types

def is_enum_type(s):
    return s in enum_types

def is_const_prim_ptr(s):
    for prim_type in prim_types:
        if s == f"const {prim_type} *":
            return True
    return False

def is_prim_ptr(s):
    for prim_type in prim_types:
        if s == f"const {prim_type} *" or s == f"{prim_type} *":
            return True
    return False

def is_const_struct_ptr(s):
    for struct_type in struct_types:
        if s == f"const {struct_type} *":
            return True
    return False

def is_struct_ptr(s):
    for struct_type in struct_types:
        if s == f"const {struct_type} *" or s == f"{struct_type} *":
            return True
    return False

def type_default_value(s):
    return prim_defaults[s]

# get C-style result of a function pointer as string
def funcptr_result_c(field_type):
    res_type = field_type[:field_type.index('(*)')].strip()
    if res_type == 'void':
        return 'void'
    elif util.is_const_void_ptr(res_type):
        return '?*const anyopaque'
    elif util.is_void_ptr(res_type):
        return '?*anyopaque'
    else:
        sys.exit(f"ERROR funcptr_result_c(): {field_type}")

def funcdecl_args_c(decl, prefix):
    s = ""
    func_name = decl['name']
    for param_decl in decl['params']:
        if s != "":
            s += ", "
        param_name = param_decl['name']
        param_type = check_override(f'{func_name}.{param_name}', default=param_decl['type'])
        s += as_c_arg_type(param_type, prefix)
    return s

def funcdecl_result_c(decl, prefix):
    func_name = decl['name']
    decl_type = decl['type']
    result_type = check_override(f'{func_name}.RESULT', default=decl_type[:decl_type.index('(')].strip())
    return as_c_arg_type(result_type, prefix)

def gen_struct(decl, namespace):
    global struct_types
    struct_name = check_override(decl['name'])
    l(f"    public static class {struct_name} {{")
    for field in decl['fields']:
        field_name = check_override(field['name'])
        jni_field_name = to_jni_camel_case(field_name, 'm_')
        field_type = check_override(f'{struct_name}.{jni_field_name}', default=field['type'])

        field_type = field_type.replace(f'{namespace}::', '')

        if is_prim_type(field_type):
            if field_name.endswith('Count') and has_array(decl['fields'], namespace, field_name.replace('Count', '')):
                    continue

            jni_type = c_type_to_java_type.get(field_type);
            suffix = ''
            if jni_type == 'float':
                suffix = 'f'
            l(f"        public {jni_type} {jni_field_name} = {type_default_value(field_type)}{suffix};")
        elif is_struct_type(field_type):
            l(f"        public {field_type} {jni_field_name};")
        elif is_enum_type(field_type):
            l(f"        public {field_type} {jni_field_name} = {field_type}.{enum_default_item(field_type)};")
        elif util.is_string_ptr(field_type):
            l(f"        public String {jni_field_name};")
        # elif util.is_const_void_ptr(field_type):
        #     l(f"        public {jni_field_name}: ?*const anyopaque = null,")
        # elif util.is_void_ptr(field_type):
        #     l(f"        public {jni_field_name}: ?*anyopaque = null,")
        elif is_prim_ptr(field_type) or is_struct_ptr(field_type):
            field_type = util.extract_ptr_type(field_type)
            jni_type = c_type_to_java_type.get(field_type, field_type)
            l(f"        public {jni_type}[] {jni_field_name};")

        elif util.is_dmarray_type(field_type):
            field_type = util.extract_dmarray_type(field_type)
            field_type = c_type_to_java_type.get(field_type, field_type)
            if is_struct_ptr(field_type):
                field_type = util.extract_ptr_type(field_type)
            l(f"        public {field_type}[] {jni_field_name};")
        # elif util.is_func_ptr(field_type):
        #     l(f"        public {jni_field_name}: ?*const fn ({funcptr_args_c(field_type, prefix)}) callconv(.C) {funcptr_result_c(field_type)} = null,")
        # elif util.is_1d_array_type(field_type):
        #     array_type = util.extract_array_type(field_type)
        #     array_sizes = util.extract_array_sizes(field_type)
        #     if is_prim_type(array_type) or is_struct_type(array_type):
        #         if is_prim_type(array_type):
        #             zig_type = as_zig_prim_type(array_type)
        #             def_val = type_default_value(array_type)
        #         elif is_struct_type(array_type):
        #             zig_type = as_zig_struct_type(array_type, prefix)
        #             def_val = '.{}'
        #         elif is_enum_type(array_type):
        #             zig_type = as_zig_enum_type(array_type, prefix)
        #             def_val = '.{}'
        #         else:
        #             sys.exit(f"ERROR gen_struct is_1d_array_type: {array_type}")
        #         t0 = f"[{array_sizes[0]}]{zig_type}"
        #         t1 = f"[_]{zig_type}"
        #         l(f"        {jni_field_name}: {t0} = {t1}{{{def_val}}} ** {array_sizes[0]},")
        #     elif util.is_const_void_ptr(array_type):
        #         l(f"        {jni_field_name}: [{array_sizes[0]}]?*const anyopaque = [_]?*const anyopaque{{null}} ** {array_sizes[0]},")
        #     else:
        #         sys.exit(f"ERROR gen_struct: array {jni_field_name}: {field_type} => {array_type} [{array_sizes[0]}]")
        # elif util.is_2d_array_type(field_type):
        #     array_type = util.extract_array_type(field_type)
        #     array_sizes = util.extract_array_sizes(field_type)
        #     if is_prim_type(array_type):
        #         zig_type = as_zig_prim_type(array_type)
        #         def_val = type_default_value(array_type)
        #     elif is_struct_type(array_type):
        #         zig_type = as_zig_struct_type(array_type, prefix)
        #         def_val = ".{}"
        #     else:
        #         sys.exit(f"ERROR gen_struct is_2d_array_type: {array_type}")
        #     t0 = f"[{array_sizes[0]}][{array_sizes[1]}]{zig_type}"
        #     l(f"        {jni_field_name}: {t0} = [_][{array_sizes[1]}]{zig_type}{{[_]{zig_type}{{{def_val}}} ** {array_sizes[1]}}} ** {array_sizes[0]},")
        else:
            sys.exit(f"ERROR gen_struct: {field_name}: {field_type};")
    l("    };")

def gen_consts(decl, prefix):
    for item in decl['items']:
        item_name = check_override(item['name'])
        l(f"pub const {util.as_lower_snake_case(item_name, prefix)} = {item['value']};")

def gen_const_var(decl, prefix):
    item_name = check_override(decl['name'])
    field_type = check_override(f'{prefix}.{item_name}', default=decl['type'])
    suffix =  ''
    if field_type == 'float':
        suffix = 'f'
    l(f"    public static final {field_type} {item_name} = {decl['value']}{suffix};")

def gen_enum(decl, prefix):
    enum_name = check_override(decl['name'])

    has_values = any(['value' in item for item in decl['items']])

    l(f'    public enum {enum_name} {{')

    last_value = 0
    count = len(decl['items'])
    for i, item in enumerate(decl['items']):
        lend = ','
        if i == (count-1):
            lend = ';'
        item_name = check_override(item['name'])
        if item_name != "FORCE_U32":
            if 'value' in item:
                l(f"        {item_name}({item['value']})" + lend)
                last_value = int(item['value'])
            else:
                l(f"        {item_name}({last_value+1})" + lend)
                last_value += 1
    l(f'        private final int value;')
    l(f'        private {enum_name}(int value) {{')
    l(f'            this.value = value;')
    l(f'        }}')
    l(f'        public int getValue() {{')
    l(f'            return this.value;')
    l(f'        }}')
    l(f'        static public {enum_name} fromValue(int value) throws IllegalArgumentException {{')
    l(f'            for ({enum_name} e : {enum_name}.values()) {{')
    l(f'                if (e.value == value)')
    l(f'                    return e;')
    l(f'            }}')
    l(f'            throw new IllegalArgumentException(String.format("Invalid value to {enum_name}: %d", value) );')
    l(f'        }}')

    l("    };")
    l("")

# def gen_func_c(decl, prefix):
#     l(f"pub extern fn {decl['name']}({funcdecl_args_c(decl, prefix)}) {funcdecl_result_c(decl, prefix)};")

# def gen_func_zig(decl, prefix):
#     c_func_name = decl['name']
#     zig_func_name = util.as_lower_camel_case(check_override(decl['name']), prefix)
#     if c_func_name in c_callbacks:
#         # a simple forwarded C callback function
#         l(f"pub const {zig_func_name} = {c_func_name};")
#     else:
#         zig_res_type = funcdecl_result_zig(decl, prefix)
#         l(f"pub fn {zig_func_name}({funcdecl_args_zig(decl, prefix)}) {zig_res_type} {{")
#         if is_zig_string(zig_res_type):
#             # special case: convert C string to Zig string slice
#             s = f"    return cStrToZig({c_func_name}("
#         elif zig_res_type != 'void':
#             s = f"    return {c_func_name}("
#         else:
#             s = f"    {c_func_name}("
#         for i, param_decl in enumerate(decl['params']):
#             if i > 0:
#                 s += ", "
#             arg_name = param_decl['name']
#             arg_type = param_decl['type']
#             if is_const_struct_ptr(arg_type):
#                 s += f"&{arg_name}"
#             elif util.is_string_ptr(arg_type):
#                 s += f"@ptrCast({arg_name})"
#             else:
#                 s += arg_name
#         if is_zig_string(zig_res_type):
#             s += ")"
#         s += ");"
#         l(s)
#         l("}")

def pre_parse(inp):
    global struct_types
    global enum_types
    for decl in inp['decls']:
        kind = decl['kind']
        if kind == 'namespace':
            pre_parse(decl)
        elif kind == 'struct':
            struct_types.append(decl['name'])
        elif kind == 'enum':
            enum_name = decl['name']
            enum_types.append(enum_name)
            enum_items[enum_name] = []
            for item in decl['items']:
                enum_items[enum_name].append(item['name'])

# def gen_imports(inp, dep_prefixes):
#     l('const builtin = @import("builtin");')
#     for dep_prefix in dep_prefixes:
#         dep_module_name = module_names[dep_prefix]
#         l(f'const {dep_prefix[:-1]} = @import("{dep_module_name}.zig");')
#     l('')

# def gen_helpers(inp):
#     l('// helper function to convert a C string to a Zig string slice')
#     l('fn cStrToZig(c_str: [*c]const u8) [:0]const u8 {')
#     l('    return @import("std").mem.span(c_str);')
#     l('}')
#     if inp['prefix'] in ['sg_', 'sdtx_', 'sshape_']:
#         l('// helper function to convert "anything" to a Range struct')
#         l('pub fn asRange(val: anytype) Range {')
#         l('    const type_info = @typeInfo(@TypeOf(val));')
#         l('    switch (type_info) {')
#         l('        .Pointer => {')
#         l('            switch (type_info.Pointer.size) {')
#         l('                .One => return .{ .ptr = val, .size = @sizeOf(type_info.Pointer.child) },')
#         l('                .Slice => return .{ .ptr = val.ptr, .size = @sizeOf(type_info.Pointer.child) * val.len },')
#         l('                else => @compileError("FIXME: Pointer type!"),')
#         l('            }')
#         l('        },')
#         l('        .Struct, .Array => {')
#         l('            @compileError("Structs and arrays must be passed as pointers to asRange");')
#         l('        },')
#         l('        else => {')
#         l('            @compileError("Cannot convert to range!");')
#         l('        },')
#         l('    }')
#         l('}')
#         l('')
#     if inp['prefix'] == 'sdtx_':
#         l('// std.fmt compatible Writer')
#         l('pub const Writer = struct {')
#         l('    pub const Error = error{};')
#         l('    pub fn writeAll(self: Writer, bytes: []const u8) Error!void {')
#         l('        _ = self;')
#         l('        for (bytes) |byte| {')
#         l('            putc(byte);')
#         l('        }')
#         l('    }')
#         l('    pub fn writeByteNTimes(self: Writer, byte: u8, n: u64) Error!void {')
#         l('        _ = self;')
#         l('        var i: u64 = 0;')
#         l('        while (i < n) : (i += 1) {')
#         l('            putc(byte);')
#         l('        }')
#         l('    }')
#         l('};')
#         l('// std.fmt-style formatted print')
#         l('pub fn print(comptime fmt: anytype, args: anytype) void {')
#         l('    var writer: Writer = .{};')
#         l('    @import("std").fmt.format(writer, fmt, args) catch {};')
#         l('}')
#         l('')

def gen_namespace(inp, class_name, package_name):
    prefix = inp.get('prefix', inp.get('name', 'unknown'))
    for decl in inp['decls']:
        # if decl.get('is_dep', False):
        kind = decl['kind']
        if kind == 'namespace':
            gen_namespace(decl, class_name, package_name)
        elif kind == 'consts':
            gen_consts(decl, prefix)
            #print(f"Not implemented yet: {kind}")
        elif kind == 'var':
            gen_const_var(decl, prefix)
        elif not check_ignore(decl['name']):
            if kind == 'struct':
                gen_struct(decl, prefix)
            elif kind == 'enum':
                gen_enum(decl, prefix)
            elif kind == 'func':
                # gen_func_c(decl, prefix)
                # gen_func_zig(decl, prefix)
                #print(f"Not implemented yet: {kind}")
                pass

def gen_java_source(inp, class_name, package_name):
    l('// generated, do not edit')
    l('')
    l(f'package {package_name};')
    l('import java.io.File;')
    l('import java.io.FileInputStream;')
    l('import java.io.InputStream;')
    l('import java.io.IOException;')
    l('import java.lang.reflect.Method;')
    l('')
    l(f'public class {class_name} {{')

    pre_parse(inp)

    prefix = inp['prefix']
    gen_namespace(inp, class_name, package_name)

    l('}') # class
    l('')

def to_jni_camel_case(s, prefix):
    s = s.replace(prefix, '')
    s = s[0].lower() + s[1:]
    return s

def gen_jni_struct(decl, namespace, header=False):
    global struct_jni_types
    struct_name = check_override(decl['name']) + 'JNI'
    struct_jni_types.append(decl)
    if header:
        l(f"struct {struct_name} {{")
        l(f"    jclass cls;")
        for field in decl['fields']:
            field_name = check_override(field['name'])
            if field_name.endswith('Count') and has_array(decl['fields'], namespace, field_name.replace('Count', '')):
                    continue

            jni_field_name = to_jni_camel_case(field_name, 'm_')
            l(f"    jfieldID {jni_field_name};")
        l("};")

def gen_jni_typeinfos(header=False):
    global struct_jni_types
    if header:
        l(f"struct TypeInfos {{")
        for decl in struct_jni_types:
            l(f"    {decl['name']}JNI m_{decl['name']}JNI;")
        l("};")

def gen_jni_type_init(namespace, class_name, package_name, header=False):
    global struct_jni_types

    fn_initialize = 'void InitializeJNITypes(JNIEnv* env, TypeInfos* infos)'
    fn_finalize = 'void FinalizeJNITypes(JNIEnv* env, TypeInfos* infos)'
    if header:
        l(f'{fn_initialize};')
        l(f'{fn_finalize};')
        l('')
        return

    l(f'{fn_initialize} {{')
    l(f'    #define SETUP_CLASS(TYPE, TYPE_NAME) \\')
    l(f'        TYPE * obj = &infos->m_ ## TYPE ; \\')
    l(f'        obj->cls = dmJNI::GetClass(env, CLASS_NAME, TYPE_NAME); \\')
    l(f'        if (!obj->cls) {{ \\')
    l(f'            printf("ERROR: Failed to get class %s$%s\\n", CLASS_NAME, TYPE_NAME); \\')
    l(f'        }}')
    l(f'')
    l(f'    #define GET_FLD_TYPESTR(NAME, FULL_TYPE_STR) \\')
    l(f'        obj-> NAME = env->GetFieldID(obj->cls, # NAME, FULL_TYPE_STR)')
    l(f'')
    l(f'    #define GET_FLD(NAME, TYPE_NAME) \\')
    l(f'        obj-> NAME = env->GetFieldID(obj->cls, # NAME, "L" CLASS_NAME "$" TYPE_NAME ";")')
    l(f'')
    l(f'    #define GET_FLD_ARRAY(NAME, TYPE_NAME) \\')
    l(f'        obj-> NAME = env->GetFieldID(obj->cls, # NAME, "[L" CLASS_NAME "$" TYPE_NAME ";")')
    l(f'')

    for decl in struct_jni_types:
        struct_name = decl["name"]

        l(f'    {{')
        l(f'        SETUP_CLASS({struct_name}JNI, "{struct_name}");')

        for field in decl['fields']:
            field_name = check_override(field['name'])
            jni_field_name = to_jni_camel_case(field_name, 'm_')
            field_type = check_override(f'{struct_name}.{field_name}', default=field['type'])

            namespace_prefix = f"{namespace}::"
            field_type = field_type.replace(namespace_prefix, '')

            #gen_ir.printtable(field)

            if is_prim_type(field_type):
                if field_name.endswith('Count') and has_array(decl['fields'], namespace, field_name.replace('Count', '')):
                    continue

                jni_type = c_type_to_jni_type_specifiers.get(field_type);
                l(f'        GET_FLD_TYPESTR({jni_field_name}, "{jni_type}");')
            elif is_struct_type(field_type):
                l(f'        GET_FLD({jni_field_name}, "{field_type}");')
            elif is_enum_type(field_type):
                l(f'        GET_FLD({jni_field_name}, "{field_type}");')
            elif util.is_string_ptr(field_type):
                l(f'        GET_FLD_TYPESTR({jni_field_name}, "Ljava/lang/String;");')
            elif is_prim_ptr(field_type): # arrays: e.g. uint8_t* data;
                field_type = util.extract_ptr_type(field_type)
                jni_type = c_type_to_jni_type_specifiers.get(field_type);
                l(f'        GET_FLD_TYPESTR({jni_field_name}, "[{jni_type}");')
            elif is_struct_ptr(field_type):
                jni_type = util.extract_ptr_type(field_type)
                l(f'        GET_FLD_ARRAY({jni_field_name}, "{jni_type}");')

            elif util.is_dmarray_type(field_type):
                field_type = util.extract_dmarray_type(field_type)

                if is_prim_type(field_type):
                    jni_type = c_type_to_jni_type_specifiers.get(field_type);
                    l(f'        GET_FLD_TYPESTR({jni_field_name}, "[{jni_type}");')
                    continue
                elif is_struct_type(field_type):
                    jni_type = util.extract_ptr_type(field_type)
                elif util.is_string_ptr(field_type):
                    jni_type = "Ljava/lang/String;"
                l(f'        GET_FLD_ARRAY({jni_field_name}, "{jni_type}");')
            else:
                print("gen_struct: Not implemented yet:", field['kind'], field_type)

        l(f'    }}')
    l('    #undef GET_FLD')
    l('    #undef GET_FLD_ARRAY')
    l('    #undef GET_FLD_TYPESTR')
    l('}')
    l('')

def get_array_count_field(decl, wanted_field_name):
    field = None
    for f in decl['fields']:
        field_name = check_override(f['name'])
        if wanted_field_name == field_name:
            field = f
            break
    if field is None:
        return None
    struct_name = decl['name']
    field_type = check_override(f'{struct_name}.{field_name}', default=field['type'])
    if not is_prim_type(field_type):
        return None
    return field

def has_array(fields, namespace, name):
    for field in fields:
        field_name = check_override(field['name'])
        if field_name == name:
            field_type = field['type']
            field_type = field_type.replace(f"{namespace}::", '')
            if is_prim_ptr(field_type) or is_struct_ptr(field_type):
                return True
    return False

# Generate create functions for the types
def gen_to_jni_create_object(decl, namespace, class_name, package_name, header=False):
    struct_name = decl["name"]

    fn = f'jobject Create{struct_name}(JNIEnv* env, TypeInfos* types, const {struct_name}* src)'
    if header:
        l(f'{fn};')
        return

    l(f'{fn} {{')
    l(f'    jobject obj = env->AllocObject(types->m_{struct_name}JNI.cls);')

    for field in decl['fields']:
        field_name = check_override(field['name'])

        jni_field_name = to_jni_camel_case(field_name, 'm_')
        field_type = check_override(f'{struct_name}.{field_name}', default=field['type'])

        namespace_prefix = f"{namespace}::"
        field_type = field_type.replace(namespace_prefix, '')

        # https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/functions.html#Set_type_Field_routines
        if is_prim_type(field_type):
            if field_name.endswith('Count') and has_array(decl['fields'], namespace, field_name.replace('Count', '')):
                continue

            jni_type = c_type_to_jni_capname.get(field_type, None)
            if jni_type is not None:
                l(f'    dmJNI::Set{jni_type}(env, obj, types->m_{struct_name}JNI.{jni_field_name}, src->{field_name});')
        elif is_struct_type(field_type):
            l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, Create{field_type}(env, types, &src->{field_name}));')
        elif is_enum_type(field_type):
            l(f'    dmJNI::SetEnum(env, obj, types->m_{struct_name}JNI.{jni_field_name}, src->{field_name});')
        elif util.is_string_ptr(field_type):
            l(f'    dmJNI::SetString(env, obj, types->m_{struct_name}JNI.{jni_field_name}, src->{field_name});')
        #elif is_const_prim_ptr(field_type):
        elif is_const_prim_ptr(field_type) or is_struct_ptr(field_type):
            # need a variable that designates the array size
            count_name = field_name+'Count'
            count = get_array_count_field(decl, count_name)
            if not count:
                print(f'Field "{field_name}" has no corresponding field "{count_name}"')
                continue

            field_type = util.extract_ptr_type(field_type)

            if is_struct_type(field_type):
                l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, Create{field_type}Array(env, types, src->{field_name}, src->{count_name}));')
                continue

            jni_typestr = c_type_to_jni_capname.get(field_type, field_type)
            l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, dmJNI::Create{jni_typestr}Array(env, src->{field_name}, src->{count_name}));')
        elif util.is_dmarray_type(field_type):
            field_type = util.extract_dmarray_type(field_type)
            jni_typestr = c_type_to_jni_capname.get(field_type, None)
            jni_type = c_type_to_jni_type.get(field_type, None)

            if is_struct_type(field_type):
                l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, Create{field_type}Array(env, types, src->{field_name}.Begin(), src->{field_name}.Size()));')
            elif is_struct_ptr(field_type):
                #field_type = util.extract_ptr_type(field_type)
                pass # not implemented yet
            else:
                l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, dmJNI::Create{jni_typestr}Array(env, src->{field_name}.Begin(), src->{field_name}.Size()));')

    l(f'    return obj;')
    l(f'}}')
    l(f'')

def gen_to_jni_create_object_array(decl, namespace, class_name, package_name, header=False):
    struct_name = decl["name"]

    fn = f'jobjectArray Create{struct_name}Array(JNIEnv* env, TypeInfos* types, const {struct_name}* src, uint32_t src_count)'
    if header:
        l(f'{fn};')
        return

    l(f'{fn} {{')
    l(f'    jobjectArray arr = env->NewObjectArray(src_count, types->m_{struct_name}JNI.cls, 0);')
    l(f'    for (uint32_t i = 0; i < src_count; ++i) {{')
    l(f'        jobject obj = Create{struct_name}(env, types, &src[i]);')
    l(f'        env->SetObjectArrayElement(arr, i, obj);')
    l(f'        env->DeleteLocalRef(obj);')
    l(f'    }}')
    l(f'    return arr;')
    l(f'}}')

def gen_from_jni_get_object(decl, namespace, class_name, package_name, header=False):
    struct_name = decl["name"]

    fn = f'bool Get{struct_name}(JNIEnv* env, TypeInfos* types, jobject obj, {struct_name}* out)'
    if header:
        l(f'{fn};')
        return

    l(f'{fn} {{')

    for field in decl['fields']:
        field_name = check_override(field['name'])

        jni_field_name = to_jni_camel_case(field_name, 'm_')
        field_type = check_override(f'{struct_name}.{field_name}', default=field['type'])

        namespace_prefix = f"{namespace}::"
        field_type = field_type.replace(namespace_prefix, '')

        field_id = f'types->m_{struct_name}JNI.{jni_field_name}'

        # https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/functions.html#Set_type_Field_routines
        if is_prim_type(field_type):
            jni_type = c_type_to_jni_capname.get(field_type, None)

            if field_name.endswith('Count') and has_array(decl['fields'], namespace, field_name.replace('Count', '')):
                continue

            if jni_type is not None:
                l(f'    out->{field_name} = dmJNI::Get{jni_type}(env, obj, {field_id});')
        elif is_struct_type(field_type):
                l(f'    {{')
                l(f'        jobject field_object = env->GetObjectField(obj, {field_id});')
                l(f'        Get{field_type}(env, types, field_object, &out->{field_name});')
                l(f'        env->DeleteLocalRef(field_object);')
                l(f'    }}')
        elif is_enum_type(field_type):
            l(f'    out->{field_name} = ({field_type})dmJNI::GetInt(env, obj, {field_id});')
        elif util.is_string_ptr(field_type):
            l(f'    out->{field_name} = dmJNI::GetString(env, obj, {field_id});')

        #     l(f'    dmJNI::SetString(env, obj, types->m_{struct_name}JNI.{jni_field_name}, src->{field_name});')
        # #elif is_const_prim_ptr(field_type):
        # elif is_const_prim_ptr(field_type) or is_struct_ptr(field_type):
        #     # need a variable that designates the array size
        #     count_name = field_name+'Count'
        #     count = get_array_count_field(decl, count_name)
        #     if not count:
        #         print(f'Field "{field_name}" has no corresponding field "{count_name}"')
        #         continue

        #     field_type = util.extract_ptr_type(field_type)

        #     if is_struct_type(field_type):
        #         l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, Create{field_type}Array(env, types, src->{field_name}, src->{count_name}));')
        #         continue

        #     jni_typestr = c_type_to_jni_capname.get(field_type, field_type)
        #     l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, dmJNI::Create{jni_typestr}Array(env, src->{field_name}, src->{count_name}));')
        # elif util.is_dmarray_type(field_type):
        #     field_type = util.extract_dmarray_type(field_type)
        #     jni_typestr = c_type_to_jni_capname.get(field_type, None)
        #     jni_type = c_type_to_jni_type.get(field_type, None)

        #     if is_struct_type(field_type):
        #         l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, Create{field_type}Array(env, types, src->{field_name}.Begin(), src->{field_name}.Size()));')
        #     elif is_struct_ptr(field_type):
        #         #field_type = util.extract_ptr_type(field_type)
        #         pass # not implemented yet
        #     else:
        #         l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, dmJNI::Create{jni_typestr}Array(env, src->{field_name}.Begin(), src->{field_name}.Size()));')

    l(f'    return true;')
    l(f'}}')
    l(f'')


def gen_jni_namespace_source(inp, class_name, package_name, header=False):
    global struct_jni_types

    prefix = inp.get('prefix', inp.get('name', 'unknown'))
    for decl in inp['decls']:
        kind = decl['kind']
        if kind == 'namespace':
            namespace = decl["name"]
            l(f'namespace {namespace} {{')
            gen_jni_namespace_source(decl, class_name, package_name, header=header)
            gen_jni_typeinfos(header=header)
            gen_jni_type_init(namespace, class_name, package_name, header=header)
            l('')
            l('//' + '-' * 40)
            l('// From C to Jni')
            l('//' + '-' * 40)
            for struct in struct_jni_types:
                gen_to_jni_create_object(struct, namespace, class_name, package_name, header=header)
            for struct in struct_jni_types:
                gen_to_jni_create_object_array(struct, namespace, class_name, package_name, header=header)
            l('//' + '-' * 40)
            l('// From Jni to C')
            l('//' + '-' * 40)
            for struct in struct_jni_types:
                gen_from_jni_get_object(struct, namespace, class_name, package_name, header=header)
            # for struct in struct_jni_types:
            #     gen_from_jni_get_object_array(struct, namespace, class_name, package_name, header=header)
            l(f'}} // {namespace}')
        # elif kind == 'consts':
        #     ##gen_consts(decl, prefix)
        #     print(f"Not implemented yet: {kind}")
        # elif kind == 'var':
        #     #gen_const_var(decl, prefix)
        #     print(f"Not implemented yet: {kind}")
        elif not check_ignore(decl['name']):
            if kind == 'struct':
                gen_jni_struct(decl, prefix, header=header)
            # else:
            #     print(f"Not implemented yet: {kind}")
            # elif kind == 'enum':
            #     gen_enum(decl, prefix)
            # elif kind == 'func':
            #     print(f"Not implemented yet: {kind}")

def gen_jni_source(inp, class_name, header_name, package_name):
    l('// generated, do not edit')
    l('')
    l('#include <jni.h>')
    l('')
    l(f'#include "{header_name}"')
    l('#include "jni_util.h"')
    l('#include <dlib/array.h>')
    l('')

    l(f'#define CLASS_NAME_FORMAT "{package_name.replace(".","/")+"/" + class_name + "$%s"}"')
    l('')
    gen_jni_namespace_source(inp, class_name, package_name, header=False)


def gen_jni_header(inp, class_name, package_name):
    l('// generated, do not edit')
    l('')
    l('#include <jni.h>')
    l(f'#include "{class_name.lower()}.h"')
    l('')
    l(f'#define JAVA_PACKAGE_NAME "{package_name.replace(".","/")}"')
    l(f'#define CLASS_NAME "{package_name.replace(".","/")+"/" + class_name}"')
    l('')
    gen_jni_namespace_source(inp, class_name, package_name, header=True)

def prepare():
    print('=== Generating Java bindings:')
    if not os.path.isdir('src/java'):
        os.makedirs('src/java')

def make_package_dir(outdir, packagename):
    out = os.path.join(outdir, packagename.replace('.', os.path.sep))
    os.makedirs(out, exist_ok=True) # com.dynamo.bob.pipeline
    return out

def generate(header_path, namespace, package_name, includes, java_outdir, jni_outdir):

    module_name = os.path.basename(header_path)
    module_name = os.path.splitext(module_name)[0].capitalize()

    package_name = 'com.dynamo.bob.pipeline'
    package_dir = make_package_dir(java_outdir, package_name)

    source_path = f'{jni_outdir}/{module_name}.empty.cpp'
    with open(source_path, 'wb') as f:
        f.write(f"#include \"{header_path}\"".encode('utf-8'))

    print(f'  {source_path} => {module_name}')
    reset_globals()
    ir = gen_ir.gen(source_path, includes, module_name, namespace, [])

    os.unlink(source_path)

    output_java_path = f"{package_dir}/{ir['module']}.java"
    output_jni_cpp_path = f"{jni_outdir}/{ir['module']}_jni.cpp"
    output_jni_h_path = f"{jni_outdir}/{ir['module']}_jni.h"

    gen_java_source(ir, module_name, package_name)

    with open(output_java_path, 'w', newline='\n') as f_outp:
        f_outp.write(out_lines)
    print("Wrote", output_java_path)

    reset_lines()
    gen_jni_source(ir, module_name, os.path.basename(output_jni_h_path), package_name)

    with open(output_jni_cpp_path, 'w', newline='\n') as f_outp:
        f_outp.write(out_lines)
    print("Wrote", output_jni_cpp_path)

    reset_lines()
    gen_jni_header(ir, module_name, package_name)
    with open(output_jni_h_path, 'w', newline='\n') as f_outp:
        f_outp.write(out_lines)
    print("Wrote", output_jni_h_path)
