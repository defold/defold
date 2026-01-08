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

# until we have a DEFOLD_HOME
if not 'DEFOLD_HOME' in os.environ:
    os.environ['DEFOLD_HOME'] = os.path.normpath(os.path.join(os.environ["DYNAMO_HOME"], '../..'))
sys.path.insert(0, os.environ['DEFOLD_HOME'])
import apply_license

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
    'int8_t':       'B',
    'uint8_t':      'B',
    'int16_t':      'S',
    'uint16_t':     'S',
    'int32_t':      'I',
    'uint32_t':     'I',
    'int64_t':      'J',
    'uint64_t':     'J',
    'float':        'F',
    'double':       'D',
    'void *':       'J',
}

# https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/functions.html#Set_type_Field_routines
c_type_to_jni_capname = {
    'bool':     'Boolean',
    'char':     'Char',
    'int8_t':   'Byte',
    'uint8_t':  'UByte',
    'int16_t':  'Short',
    'uint16_t': 'UShort',
    'int':      'Int',
    'int32_t':  'Int',
    'uint32_t': 'UInt', # same as Int, but we do a cast internally
    'int64_t':  'Long',
    'uint64_t': 'ULong',
    'float':    'Float',
    'double':   'Double',
}

c_type_to_jni_type = {
    'bool':     'jboolean',
    'char':     'jchar',
    'int8_t':   'jbyte',
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
    'int8_t':   'byte',
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

def is_prim_ptr(s):
    c_type, _ = util.extract_ptr_type2(s)
    return c_type in prim_types

def is_const_prim_ptr(s):
    return is_prim_ptr(s) and 'const' in s

def is_struct_ptr(s):
    c_type, _ = util.extract_ptr_type2(s)
    return c_type in struct_types

def is_const_struct_ptr(s):
    return is_struct_ptr(s) and 'const' in s


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
        elif util.is_const_void_ptr(field_type) or util.is_void_ptr(field_type):
            l(f"        public long {jni_field_name};")
        elif util.is_dmarray_type(field_type):
            field_type = util.extract_dmarray_type(field_type)
            c_type, _ = util.extract_ptr_type2(field_type)
            java_type = c_type_to_java_type.get(c_type, c_type)
            l(f"        public {java_type}[] {jni_field_name};")

        elif is_prim_ptr(field_type) or is_struct_ptr(field_type):
            c_type, _ = util.extract_ptr_type2(field_type)
            jni_type = c_type_to_java_type.get(c_type, c_type)

            # Checking if it's an array: need a variable that designates the array size
            count_name = field_name+'Count'
            count = get_array_count_field(decl, count_name)

            if not count:
                l(f"        public {jni_type} {jni_field_name};")
            else:
                l(f"        public {jni_type}[] {jni_field_name};")

        elif util.is_1d_array_type(field_type):
            array_type = util.extract_array_type(field_type)
            array_sizes = util.extract_array_sizes(field_type)

            if is_prim_ptr(array_type) or is_struct_ptr(array_type):
                c_type, _ = util.extract_ptr_type2(array_type)
                jni_type = c_type_to_java_type.get(c_type, c_type)

                l(f"        public {jni_type}[] {jni_field_name};")

        elif util.is_func_ptr(field_type):
            print(f"Function pointers aren't yet supported. Skipping {struct_name}.{field_name}")
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

    last_value = -1
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

def gen_namespace(inp, class_name, package_name):
    prefix = inp.get('prefix', inp.get('name', 'unknown'))
    for decl in inp['decls']:
        kind = decl['kind']
        if kind == 'namespace':
            gen_namespace(decl, class_name, package_name)
        elif kind == 'consts':
            gen_consts(decl, prefix)
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
    l(apply_license.get_license_for_file("foo.java"))
    l('')
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
    fn_finalize   = 'void FinalizeJNITypes(JNIEnv* env, TypeInfos* infos)'
    if header:

        l(f'{fn_initialize};')
        l(f'{fn_finalize};')

        l('')
        l(f'struct ScopedContext {{')
        l(f'    JNIEnv*   m_Env;')
        l(f'    TypeInfos m_TypeInfos;')
        l(f'    ScopedContext(JNIEnv* env) : m_Env(env) {{')
        l(f'        InitializeJNITypes(m_Env, &m_TypeInfos);')
        l(f'    }}')
        l(f'    ~ScopedContext() {{')
        l(f'        FinalizeJNITypes(m_Env, &m_TypeInfos);')
        l(f'    }}')
        l(f'}};')

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
    l(f'        obj-> NAME = dmJNI::GetFieldFromString(env, obj->cls, # NAME, FULL_TYPE_STR);')
    l(f'')
    l(f'    #define GET_FLD(NAME, TYPE_NAME) \\')
    l(f'        obj-> NAME = dmJNI::GetFieldFromString(env, obj->cls, # NAME, "L" CLASS_NAME "$" TYPE_NAME ";")')
    l(f'')
    l(f'    #define GET_FLD_ARRAY(NAME, TYPE_NAME) \\')
    l(f'        obj-> NAME = dmJNI::GetFieldFromString(env, obj->cls, # NAME, "[L" CLASS_NAME "$" TYPE_NAME ";")')
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
            elif util.is_const_void_ptr(field_type) or util.is_void_ptr(field_type):
                jni_type = c_type_to_jni_type_specifiers.get(field_type);
                l(f'        GET_FLD_TYPESTR({jni_field_name}, "{jni_type}");')

            elif is_struct_ptr(field_type):
                c_type, ptr = util.extract_ptr_type2(field_type)
                if ptr and len(ptr) > 2:
                    print(f"Multiple indirections are not supported: {struct_name}: {field_type} {field_name}")
                    continue

                count_name = field_name+'Count'
                count = get_array_count_field(decl, count_name)

                if not count:
                    l(f'        GET_FLD({jni_field_name}, "{c_type}");')
                else:
                    l(f'        GET_FLD_ARRAY({jni_field_name}, "{c_type}");')

            elif util.is_dmarray_type(field_type):
                field_type = util.extract_dmarray_type(field_type)
                c_type, ptr = util.extract_ptr_type2(field_type)

                if ptr and len(ptr) > 2:
                    print(f"Multiple indirections are not supported: {struct_name}: {field_type} {field_name}")
                    continue

                if is_prim_type(c_type) and not ptr:
                    jni_type = c_type_to_jni_type_specifiers.get(c_type);
                    l(f'        GET_FLD_TYPESTR({jni_field_name}, "[{jni_type}");')
                    continue
                elif is_struct_type(c_type):
                    jni_type = c_type

                elif util.is_string_ptr(field_type):
                    jni_type = "Ljava/lang/String;"
                l(f'        GET_FLD_ARRAY({jni_field_name}, "{jni_type}");')
            elif is_prim_ptr(field_type): # arrays: e.g. uint8_t* data;
                c_type, ptr = util.extract_ptr_type2(field_type)
                if ptr and len(ptr) > 1:
                    print(f"Multiple indirections are not supported: {struct_name}: {field_type} {field_name}")
                    continue

                jni_type = c_type_to_jni_type_specifiers.get(c_type);
                l(f'        GET_FLD_TYPESTR({jni_field_name}, "[{jni_type}");')

            elif util.is_1d_array_type(field_type):
                array_type = util.extract_array_type(field_type)
                array_sizes = util.extract_array_sizes(field_type)

                if is_prim_ptr(array_type) or is_struct_ptr(array_type):
                    c_type, ptr = util.extract_ptr_type2(array_type)

                    if is_prim_type(c_type) and not ptr:
                        jni_type = c_type_to_jni_type_specifiers.get(c_type);
                        l(f'        GET_FLD_TYPESTR({jni_field_name}, "[{jni_type}");')
                        continue
                    elif is_struct_type(c_type):
                        jni_type = c_type

                    elif util.is_string_ptr(field_type):
                        jni_type = "Ljava/lang/String;"

                    l(f'        GET_FLD_ARRAY({jni_field_name}, "{jni_type}");')

            else:
                print("gen_struct: Not implemented yet:", field.get('name', ''),  field.get('kind', ''), field_type)

        l(f'    }}')
    l('    #undef GET_FLD')
    l('    #undef GET_FLD_ARRAY')
    l('    #undef GET_FLD_TYPESTR')
    l('}')
    l('')

    l(f'{fn_finalize} {{')

    for decl in struct_jni_types:
        l(f'    env->DeleteLocalRef(infos->m_{decl["name"]}JNI.cls);')
    l(f'}}')
    l(f'')


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

    fn = f'jobject C2J_Create{struct_name}(JNIEnv* env, TypeInfos* types, const {struct_name}* src)'
    if header:
        l(f'{fn};')
        return

    l(f'{fn} {{')
    l(f'    if (src == 0) return 0;')
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
            l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, C2J_Create{field_type}(env, types, &src->{field_name}));')
        elif is_enum_type(field_type):
            l(f'    dmJNI::SetEnum(env, obj, types->m_{struct_name}JNI.{jni_field_name}, src->{field_name});')
        elif util.is_string_ptr(field_type):
            l(f'    dmJNI::SetString(env, obj, types->m_{struct_name}JNI.{jni_field_name}, src->{field_name});')
        elif util.is_const_void_ptr(field_type) or util.is_void_ptr(field_type):
            l(f'    dmJNI::SetLong(env, obj, types->m_{struct_name}JNI.{jni_field_name}, (uintptr_t)src->{field_name});')

        elif util.is_dmarray_type(field_type):
            field_type = util.extract_dmarray_type(field_type)
            c_type, ptr = util.extract_ptr_type2(field_type)

            jni_typestr = c_type_to_jni_capname.get(c_type, None)
            jni_type = c_type_to_jni_type.get(c_type, None)

            # We allow dmArray<Type> or dmArray<Type*>. I.e. len(ptr) == 0 or 1
            # We allow dmArray<uint8_t>. I.e. len(ptr) == 0 or 1
            if not ptr:
                if is_struct_type(c_type):
                    l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, C2J_Create{c_type}Array(env, types, src->{field_name}.Begin(), src->{field_name}.Size()));')
                else:
                    l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, dmJNI::C2J_Create{jni_typestr}Array(env, src->{field_name}.Begin(), src->{field_name}.Size()));')

            else:
                if len(ptr) > 1:
                    print(f"Multiple indirections are not supported: {struct_name}: {field_type} {field_name}")
                    continue

                if is_struct_ptr(field_type):
                    l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, C2J_Create{c_type}PtrArray(env, types, src->{field_name}.Begin(), src->{field_name}.Size()));')
                else:
                    print(f"C2J: Arrays of primitive pointers aren't supported: {struct_name}: {field_type} {field_name}")
                    continue

        elif util.is_1d_array_type(field_type): # int data[3]
            array_type = util.extract_array_type(field_type)
            array_sizes = util.extract_array_sizes(field_type)

            if is_prim_ptr(array_type) or is_struct_ptr(array_type):

                c_type, ptr = util.extract_ptr_type2(array_type)

                jni_typestr = c_type_to_jni_capname.get(c_type, None)
                jni_type = c_type_to_jni_type.get(c_type, None)

                if not ptr:
                    if is_struct_type(c_type):
                        l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, C2J_Create{c_type}Array(env, types, src->{field_name}, {array_sizes[0]}));')
                    else:
                        l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, dmJNI::C2J_Create{jni_typestr}Array(env, src->{field_name}, {array_sizes[0]}));')
                else:
                    if len(ptr) > 1:
                        print(f"Multiple indirections are not supported: {struct_name}: {field_type} {field_name}")
                        continue

                    if is_struct_ptr(array_type):
                        l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, C2J_Create{c_type}PtrArray(env, types, src->{field_name},{array_sizes[0]}));')
                    else:
                        print(f"C2J: Arrays of primitive pointers aren't supported: {struct_name}: {field_type} {field_name}")
                        continue

        elif is_prim_ptr(field_type) or is_struct_ptr(field_type):
            c_type, ptr = util.extract_ptr_type2(field_type)

            # Checking if it's an array: need a variable that designates the array size
            count_name = field_name+'Count'
            count = get_array_count_field(decl, count_name)

            # We allow Type* or Type**. I.e. len(ptr) == 1 or 2
            # We allow uint8_t*. I.e. len(ptr) == 1

            if not count: # not an array
                if len(ptr) > 1:
                    print(f"C2J: Objects of too many indirections aren't supported: {struct_name}.{field_name} - {field_type}")
                    continue # not supported

                if is_struct_type(c_type):
                    l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, C2J_Create{c_type}(env, types, src->{field_name}));')
                else:
                    print(f"C2J: Pointer to single primitives aren't supported: {struct_name}.{field_name} - {field_type}")
                    continue # not supported

            elif is_struct_type(c_type):
                if len(ptr) == 2:
                    l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, C2J_Create{c_type}PtrArray(env, types, src->{field_name}, src->{count_name}));')
                elif len(ptr) == 1:
                    l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, C2J_Create{c_type}Array(env, types, src->{field_name}, src->{count_name}));')
                else:
                    print(f"C2J: Arrays of too many indirections aren't supported: {struct_name}.{field_name} - {field_type}")
                    continue # not supported

            else:
                if len(ptr) == 1:
                    jni_typestr = c_type_to_jni_capname.get(c_type, c_type)
                    l(f'    dmJNI::SetObjectDeref(env, obj, types->m_{struct_name}JNI.{jni_field_name}, dmJNI::C2J_Create{jni_typestr}Array(env, src->{field_name}, src->{count_name}));')
                else:
                    print(f"C2J: Arrays of too many indirections aren't supported: {struct_name}.{field_name} - {field_type}")
                    continue # not supported

    l(f'    return obj;')
    l(f'}}')
    l(f'')

def gen_to_jni_create_object_array(decl, namespace, class_name, package_name, header=False):
    struct_name = decl["name"]

    fn = f'jobjectArray C2J_Create{struct_name}Array(JNIEnv* env, TypeInfos* types, const {struct_name}* src, uint32_t src_count)'
    if header:
        l(f'{fn};')
        return

    l(f'{fn} {{')
    l(f'    if (src == 0 || src_count == 0) return 0;')
    l(f'    jobjectArray arr = env->NewObjectArray(src_count, types->m_{struct_name}JNI.cls, 0);')
    l(f'    for (uint32_t i = 0; i < src_count; ++i) {{')
    l(f'        jobject obj = C2J_Create{struct_name}(env, types, &src[i]);')
    l(f'        env->SetObjectArrayElement(arr, i, obj);')
    l(f'        env->DeleteLocalRef(obj);')
    l(f'    }}')
    l(f'    return arr;')
    l(f'}}')

def gen_to_jni_create_object_ptr_array(decl, namespace, class_name, package_name, header=False):
    struct_name = decl["name"]

    fn = f'jobjectArray C2J_Create{struct_name}PtrArray(JNIEnv* env, TypeInfos* types, const {struct_name}* const* src, uint32_t src_count)'
    if header:
        l(f'{fn};')
        return

    l(f'{fn} {{')
    l(f'    if (src == 0 || src_count == 0) return 0;')
    l(f'    jobjectArray arr = env->NewObjectArray(src_count, types->m_{struct_name}JNI.cls, 0);')
    l(f'    for (uint32_t i = 0; i < src_count; ++i) {{')
    l(f'        jobject obj = C2J_Create{struct_name}(env, types, src[i]);')
    l(f'        env->SetObjectArrayElement(arr, i, obj);')
    l(f'        env->DeleteLocalRef(obj);')
    l(f'    }}')
    l(f'    return arr;')
    l(f'}}')

def gen_from_jni_create_object(decl, namespace, class_name, package_name, header=False):
    struct_name = decl["name"]

    fn = f'bool J2C_Create{struct_name}(JNIEnv* env, TypeInfos* types, jobject obj, {struct_name}* out)'
    if header:
        l(f'{fn};')
        return

    l(f'{fn} {{')
    l(f'    if (out == 0) return false;')

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
                l(f'        if (field_object) {{')
                l(f'            J2C_Create{field_type}(env, types, field_object, &out->{field_name});')
                l(f'            env->DeleteLocalRef(field_object);')
                l(f'        }}')
                l(f'    }}')

        elif is_enum_type(field_type):
            l(f'    out->{field_name} = ({field_type})dmJNI::GetEnum(env, obj, {field_id});')

        elif util.is_string_ptr(field_type):
            l(f'    out->{field_name} = dmJNI::GetString(env, obj, {field_id});')
        elif util.is_const_void_ptr(field_type):
            l(f'    out->{field_name} = (const void*)(uintptr_t)dmJNI::GetLong(env, obj, {field_id});')
        elif util.is_void_ptr(field_type):
            l(f'    out->{field_name} = (void*)(uintptr_t)dmJNI::GetLong(env, obj, {field_id});')

        elif util.is_dmarray_type(field_type):

            field_type = util.extract_dmarray_type(field_type)
            c_type, ptr = util.extract_ptr_type2(field_type)

            jni_typestr = c_type_to_jni_capname.get(field_type, None)
            jni_type = c_type_to_jni_type.get(field_type, None)
            jni_field_name = to_jni_camel_case(field_name, 'm_')

            # We allow dmArray<Type> or dmArray<Type*>. I.e. len(ptr) == 0 or 1
            # We allow dmArray<uint8_t>. I.e. len(ptr) == 0
            if len(ptr) > 1:
                print(f"Multiple indirections are not supported: {struct_name}: {field_type} {field_name}")
                continue

            l(f'    {{')
            l(f'        jobject field_object = env->GetObjectField(obj, types->m_{struct_name}JNI.{jni_field_name});')
            l(f'        if (field_object) {{')
            l(f'            uint32_t tmp_count;')

            if not ptr:
                if is_struct_type(c_type):
                    l(f'            {c_type}* tmp = J2C_Create{c_type}Array(env, types, (jobjectArray)field_object, &tmp_count);')
                else:
                    l(f'            {c_type}* tmp = dmJNI::J2C_Create{jni_typestr}Array(env, ({jni_type}Array)field_object, &tmp_count);')
            else:
                if is_struct_type(c_type):
                    l(f'            {c_type}** tmp = J2C_Create{c_type}PtrArray(env, types, (jobjectArray)field_object, &tmp_count);')
                else:
                    print(f"C2J: Arrays of primitive pointers aren't supported: {struct_name}: {field_type} {field_name}")
                    continue

            l(f'            out->{field_name}.Set(tmp, tmp_count, tmp_count, false);')
            l(f'            env->DeleteLocalRef(field_object);')
            l(f'        }}')
            l(f'    }}')

        elif util.is_1d_array_type(field_type):
            array_type = util.extract_array_type(field_type)
            array_sizes = util.extract_array_sizes(field_type)
            c_type, ptr = util.extract_ptr_type2(array_type)

            if is_prim_ptr(array_type) or is_struct_ptr(array_type):

                jni_typestr = c_type_to_jni_capname.get(array_type, None)
                jni_type = c_type_to_jni_type.get(array_type, None)
                jni_field_name = to_jni_camel_case(field_name, 'm_')

                l(f'    {{')
                l(f'        jobject field_object = env->GetObjectField(obj, types->m_{struct_name}JNI.{jni_field_name});')
                l(f'        if (field_object) {{')

                if not ptr:
                    if is_struct_type(c_type):
                        l(f'            J2C_Create{c_type}ArrayInPlace(env, types, (jobjectArray)field_object, out->{field_name}, {array_sizes[0]});')
                    else:
                        l(f'            dmJNI::J2C_Copy{jni_typestr}Array(env, ({jni_type}Array)field_object, out->{field_name}, {array_sizes[0]});')
                else:
                    if is_struct_type(c_type):
                        l(f'            J2C_Create{c_type}PtrArrayInPlace(env, types, (jobjectArray)field_object, out->{field_name}, {array_sizes[0]});')
                    else:
                        print(f"C2J: Arrays of primitive pointers aren't supported: {struct_name}: {field_type} {field_name}")
                        continue

                l(f'            env->DeleteLocalRef(field_object);')
                l(f'        }}')
                l(f'    }}')

        elif is_prim_ptr(field_type) or is_struct_ptr(field_type):

            field_type = field_type.replace(namespace_prefix, '')

            jni_field_name = to_jni_camel_case(field_name, 'm_')

            c_type, ptr = util.extract_ptr_type2(field_type)

            count_name = field_name+'Count'
            count = get_array_count_field(decl, count_name)

            # We support Struct* and Struct**, i.e. len(ptr) is 1 or 2
            # We support uint8_t*, i.e. len(ptr) is 1 or 2

            if not count: # not an array
                if len(ptr) > 1:
                    print(f"J2C: Objects of too many indirections aren't supported: {struct_name}.{field_name} - {field_type}")
                    continue # not supported
                if not is_struct_type(c_type):
                    print(f"J2C: Pointer to single primitives aren't supported: {struct_name}.{field_name} - {field_type}")
                    continue # not supported

            elif is_struct_type(c_type):
                if len(ptr) > 2:
                    print(f"J2C: Too many indirections: {struct_name}.{field_name}: '{field_type}'")
                    continue
            else:
                if len(ptr) > 1:
                    print(f"J2C: Too many indirections: {struct_name}.{field_name}: '{field_type}'")
                    continue

            jni_typestr = c_type_to_jni_capname.get(c_type, c_type)
            jni_type = c_type_to_jni_type.get(c_type, None)

            l(f'    {{')
            l(f'        jobject field_object = env->GetObjectField(obj, types->m_{struct_name}JNI.{jni_field_name});')
            l(f'        if (field_object) {{')

            if not count: # not an array
                if is_struct_type(c_type):
                    l(f'            out->{field_name} = new {c_type}();')
                    l(f'            J2C_Create{c_type}(env, types, field_object, out->{field_name});')

            elif is_struct_type(c_type):
                if len(ptr) == 2:
                    l(f'            out->{field_name} = J2C_Create{c_type}PtrArray(env, types, (jobjectArray)field_object, &out->{count_name});')
                elif len(ptr) == 1:
                    l(f'            out->{field_name} = J2C_Create{c_type}Array(env, types, (jobjectArray)field_object, &out->{count_name});')
            else:
                if len(ptr) == 1:
                    l(f'            out->{field_name} = dmJNI::J2C_Create{jni_typestr}Array(env, ({jni_type}Array)field_object, &out->{count_name});')

            l(f'            env->DeleteLocalRef(field_object);')
            l(f'        }}')
            l(f'    }}')

    l(f'    return true;')
    l(f'}}')
    l(f'')

def gen_from_jni_create_object_array(decl, namespace, class_name, package_name, header=False):
    struct_name = decl["name"]

    fn          = f'{struct_name}* J2C_Create{struct_name}Array(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count)'
    fn_inplace  = f'void J2C_Create{struct_name}ArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, {struct_name}* dst, uint32_t dst_count)'
    if header:
        l(f'{fn};')
        l(f'{fn_inplace};')
        return

    l(f'{fn_inplace} {{')
    l(f'    jsize len = env->GetArrayLength(arr);')
    l(f'    if (len != dst_count) {{')
    l(f'        printf("Number of elements mismatch. Expected %u, but got %u\\n", dst_count, len);')
    l(f'    }}')
    l(f'    if (len > dst_count)')
    l(f'        len = dst_count;')
    l(f'    for (uint32_t i = 0; i < len; ++i) {{')
    l(f'        jobject obj = env->GetObjectArrayElement(arr, i);')
    l(f'        J2C_Create{struct_name}(env, types, obj, &dst[i]);')
    l(f'        env->DeleteLocalRef(obj);')
    l(f'    }}')
    l(f'}}')

    l(f'{fn} {{')
    l(f'    jsize len = env->GetArrayLength(arr);')
    l(f'    {struct_name}* out = new {struct_name}[len];')
    l(f'    J2C_Create{struct_name}ArrayInPlace(env, types, arr, out, len);')
    l(f'    *out_count = (uint32_t)len;')
    l(f'    return out;')
    l(f'}}')

def gen_from_jni_create_object_ptr_array(decl, namespace, class_name, package_name, header=False):
    struct_name = decl["name"]

    fn          = f'{struct_name}** J2C_Create{struct_name}PtrArray(JNIEnv* env, TypeInfos* types, jobjectArray arr, uint32_t* out_count)'
    fn_inplace  = f'void J2C_Create{struct_name}PtrArrayInPlace(JNIEnv* env, TypeInfos* types, jobjectArray arr, {struct_name}** dst, uint32_t dst_count)'
    if header:
        l(f'{fn};')
        l(f'{fn_inplace};')
        return

    l(f'{fn_inplace} {{')
    l(f'    jsize len = env->GetArrayLength(arr);')
    l(f'    if (len != dst_count) {{')
    l(f'        printf("Number of elements mismatch. Expected %u, but got %u\\n", dst_count, len);')
    l(f'    }}')
    l(f'    if (len > dst_count)')
    l(f'        len = dst_count;')

    l(f'    for (uint32_t i = 0; i < len; ++i) {{')
    l(f'        jobject obj = env->GetObjectArrayElement(arr, i);')
    l(f'        dst[i] = new {struct_name}();')
    l(f'        J2C_Create{struct_name}(env, types, obj, dst[i]);')
    l(f'        env->DeleteLocalRef(obj);')
    l(f'    }}')
    l(f'}}')

    l(f'{fn} {{')
    l(f'    jsize len = env->GetArrayLength(arr);')
    l(f'    {struct_name}** out = new {struct_name}*[len];')
    l(f'    J2C_Create{struct_name}PtrArrayInPlace(env, types, arr, out, len);')
    l(f'    *out_count = (uint32_t)len;')
    l(f'    return out;')
    l(f'}}')

def gen_jni_namespace_source(inp, class_name, package_name, header=False):
    global struct_jni_types

    prefix = inp.get('prefix', inp.get('name', 'unknown'))
    for decl in inp['decls']:
        kind = decl['kind']
        if kind == 'namespace':
            namespace = decl["name"]
            l(f'namespace {namespace} {{')
            l(f'namespace jni {{')
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
                gen_to_jni_create_object_ptr_array(struct, namespace, class_name, package_name, header=header)
            l('//' + '-' * 40)
            l('// From Jni to C')
            l('//' + '-' * 40)
            for struct in struct_jni_types:
                gen_from_jni_create_object(struct, namespace, class_name, package_name, header=header)
            for struct in struct_jni_types:
                gen_from_jni_create_object_array(struct, namespace, class_name, package_name, header=header)
                gen_from_jni_create_object_ptr_array(struct, namespace, class_name, package_name, header=header)
            l(f'}} // jni')
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
    l(apply_license.get_license_for_file("foo.cpp"))
    l('')
    l('// generated, do not edit')
    l('')
    l('#include <jni.h>')
    l('')
    l(f'#include "{header_name}"')
    l('#include <jni/jni_util.h>')
    l('#include <dlib/array.h>')
    l('')

    l(f'#define CLASS_NAME_FORMAT "{package_name.replace(".","/")+"/" + class_name + "$%s"}"')
    l('')
    gen_jni_namespace_source(inp, class_name, package_name, header=False)


def gen_jni_header(inp, class_name, package_name):
    l(apply_license.get_license_for_file("foo.h"))
    l('')
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

    source_dir = os.path.dirname(source_path)
    if not os.path.exists(source_dir):
        os.makedirs(source_dir)

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
