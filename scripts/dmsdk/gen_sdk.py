#-------------------------------------------------------------------------------
#   Generate Java+Jni bindings from C++
#
#   C++ coding style:
#   ...
#   Notes
#   - functions are not supported
#-------------------------------------------------------------------------------

import argparse
import os, sys, json, pprint, re
#sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'external'))

import gen_ir
import gen_util as util
import gen_cpp

# until we have a DEFOLD_HOME
sys.path.insert(0, os.path.normpath(os.path.join(os.environ["DYNAMO_HOME"], '../..')))
import apply_license

# ignores = [
#     # 'sdtx_printf',
#     # ...
# ]

# # NOTE: syntax for function results: "func_name.RESULT"
# overrides = {
#     # 'sgl_error':                            'sgl_get_error',   # 'error' is reserved in Zig
#     # 'sgl_deg':                              'sgl_as_degrees',
#     # 'sgl_rad':                              'sgl_as_radians',
#     # 'sg_context_desc.color_format':         'int',
#     # 'sg_context_desc.depth_format':         'int',
#     # 'sg_apply_uniforms.ub_index':           'uint32_t',
#     # 'sg_draw.base_element':                 'uint32_t',
#     # 'sg_draw.num_elements':                 'uint32_t',
#     # 'sg_draw.num_instances':                'uint32_t',
#     # 'sshape_element_range_t.base_element':  'uint32_t',
#     # 'sshape_element_range_t.num_elements':  'uint32_t',
#     # 'sdtx_font.font_index':                 'uint32_t',
#     # 'SGL_NO_ERROR':                         'SGL_ERROR_NO_ERROR',
# }

# prim_types = {
#     'int':          'i32',
#     'bool':         'bool',
#     'char':         'u8',
#     'int8_t':       'i8',
#     'uint8_t':      'u8',
#     'int16_t':      'i16',
#     'uint16_t':     'u16',
#     'int32_t':      'i32',
#     'uint32_t':     'u32',
#     'int64_t':      'i64',
#     'uint64_t':     'u64',
#     'float':        'f32',
#     'double':       'f64',
#     'uintptr_t':    'usize',
#     'intptr_t':     'isize',
#     'size_t':       'usize'
# }

# prim_defaults = {
#     'int':          '0',
#     'bool':         'false',
#     'int8_t':       '0',
#     'uint8_t':      '0',
#     'int16_t':      '0',
#     'uint16_t':     '0',
#     'int32_t':      '0',
#     'uint32_t':     '0',
#     'int64_t':      '0',
#     'uint64_t':     '0',
#     'float':        '0.0',
#     'double':       '0.0',
#     'uintptr_t':    '0',
#     'intptr_t':     '0',
#     'size_t':       '0'
# }

# # https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/types.html
# c_type_to_jni_type_specifiers = {
#     'int':          'I',
#     'bool':         'Z',
#     'int8_t':       'B',
#     'uint8_t':      'B',
#     'int16_t':      'S',
#     'uint16_t':     'S',
#     'int32_t':      'I',
#     'uint32_t':     'I',
#     'int64_t':      'J',
#     'uint64_t':     'J',
#     'float':        'F',
#     'double':       'D',
#     'void *':       'J',
# }

# # https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/functions.html#Set_type_Field_routines
# c_type_to_jni_capname = {
#     'bool':     'Boolean',
#     'char':     'Char',
#     'int8_t':   'Byte',
#     'uint8_t':  'Byte',
#     'int16_t':  'Short',
#     'uint16_t': 'Short',
#     'int':      'Int',
#     'int32_t':  'Int',
#     'uint32_t': 'Int',
#     'int64_t':  'Long',
#     'uint64_t': 'Long',
#     'float':    'Float',
#     'double':   'Double',
# }

# c_type_to_jni_type = {
#     'bool':     'jboolean',
#     'char':     'jchar',
#     'int8_t':   'jbyte',
#     'uint8_t':  'jbyte',
#     'int16_t':  'jshort',
#     'uint16_t': 'jshort',
#     'int':      'jint',
#     'int32_t':  'jint',
#     'uint32_t': 'jint',
#     'int64_t':  'jlong',
#     'uint64_t': 'jlong',
#     'float':    'jfloat',
#     'double':   'jdouble',
# }

# c_type_to_java_type = {
#     'bool':     'boolean',
#     'char':     'char',
#     'int8_t':   'byte',
#     'uint8_t':  'byte',
#     'int16_t':  'short',
#     'uint16_t': 'short',
#     'int':      'int',
#     'int32_t':  'int',
#     'uint32_t': 'int',
#     'int64_t':  'long',
#     'uint64_t': 'long',
#     'float':    'float',
#     'double':   'double',
# }

# struct_types = []
# enum_types = []
# enum_items = {}
# out_lines = ''
# struct_jni_types = []

# def reset_lines():
#     global out_lines
#     global struct_jni_types
#     out_lines = ''
#     struct_jni_types = []

# def reset_globals():
#     global struct_types
#     global enum_types
#     global enum_items
#     global struct_jni_types
#     struct_types = []
#     enum_types = []
#     enum_items = {}
#     struct_jni_types = []
#     reset_lines()

# def l(s):
#     global out_lines
#     out_lines += s + '\n'

# def as_zig_prim_type(s):
#     return prim_types[s]

# # prefix_bla_blub(_t) => (dep.)BlaBlub
# def as_zig_struct_type(s, prefix):
#     parts = s.lower().split('_')
#     outp = '' if s.startswith(prefix) else f'{parts[0]}.'
#     for part in parts[1:]:
#         # ignore '_t' type postfix
#         if (part != 't'):
#             outp += part.capitalize()
#     return outp

# # prefix_bla_blub(_t) => (dep.)BlaBlub
# def as_zig_enum_type(s, prefix):
#     parts = s.lower().split('_')
#     outp = '' if s.startswith(prefix) else f'{parts[0]}.'
#     for part in parts[1:]:
#         if (part != 't'):
#             outp += part.capitalize()
#     return outp

# def check_override(name, default=None):
#     if name in overrides:
#         return overrides[name]
#     elif default is None:
#         return name
#     else:
#         return default

# def check_ignore(name):
#     return name in ignores

# # PREFIX_ENUM_BLA => Bla, _PREFIX_ENUM_BLA => Bla
# def as_enum_item_name(s):
#     outp = s.lstrip('_')
#     parts = outp.split('_')[2:]
#     if not parts:
#         return s
#     outp = '_'.join(parts)
#     if outp[0].isdigit():
#         outp = '_' + outp
#     return outp

# def enum_default_item(enum_name):
#     return enum_items[enum_name][0]

# def is_prim_type(s):
#     return s in prim_types

# def is_struct_type(s):
#     return s in struct_types

# def is_enum_type(s):
#     return s in enum_types

# def is_prim_ptr(s):
#     c_type, _ = util.extract_ptr_type2(s)
#     return c_type in prim_types

# def is_const_prim_ptr(s):
#     return is_prim_ptr(s) and 'const' in s

# def is_struct_ptr(s):
#     c_type, _ = util.extract_ptr_type2(s)
#     return c_type in struct_types

# def is_const_struct_ptr(s):
#     return is_struct_ptr(s) and 'const' in s


# def type_default_value(s):
#     return prim_defaults[s]

# # get C-style result of a function pointer as string
# def funcptr_result_c(field_type):
#     res_type = field_type[:field_type.index('(*)')].strip()
#     if res_type == 'void':
#         return 'void'
#     elif util.is_const_void_ptr(res_type):
#         return '?*const anyopaque'
#     elif util.is_void_ptr(res_type):
#         return '?*anyopaque'
#     else:
#         sys.exit(f"ERROR funcptr_result_c(): {field_type}")

# def funcdecl_args_c(decl, prefix):
#     s = ""
#     func_name = decl['name']
#     for param_decl in decl['params']:
#         if s != "":
#             s += ", "
#         param_name = param_decl['name']
#         param_type = check_override(f'{func_name}.{param_name}', default=param_decl['type'])
#         s += as_c_arg_type(param_type, prefix)
#     return s

# def funcdecl_result_c(decl, prefix):
#     func_name = decl['name']
#     decl_type = decl['type']
#     result_type = check_override(f'{func_name}.RESULT', default=decl_type[:decl_type.index('(')].strip())
#     return as_c_arg_type(result_type, prefix)

# def gen_struct(decl, namespace):
#     global struct_types
#     struct_name = check_override(decl['name'])
#     l(f"    public static class {struct_name} {{")
#     for field in decl['fields']:
#         field_name = check_override(field['name'])
#         jni_field_name = to_jni_camel_case(field_name, 'm_')
#         field_type = check_override(f'{struct_name}.{jni_field_name}', default=field['type'])

#         field_type = field_type.replace(f'{namespace}::', '')

#         if is_prim_type(field_type):
#             if field_name.endswith('Count') and has_array(decl['fields'], namespace, field_name.replace('Count', '')):
#                     continue

#             jni_type = c_type_to_java_type.get(field_type);
#             suffix = ''
#             if jni_type == 'float':
#                 suffix = 'f'
#             l(f"        public {jni_type} {jni_field_name} = {type_default_value(field_type)}{suffix};")
#         elif is_struct_type(field_type):
#             l(f"        public {field_type} {jni_field_name};")
#         elif is_enum_type(field_type):
#             l(f"        public {field_type} {jni_field_name} = {field_type}.{enum_default_item(field_type)};")
#         elif util.is_string_ptr(field_type):
#             l(f"        public String {jni_field_name};")
#         elif util.is_const_void_ptr(field_type) or util.is_void_ptr(field_type):
#             l(f"        public long {jni_field_name};")
#         elif util.is_dmarray_type(field_type):
#             field_type = util.extract_dmarray_type(field_type)
#             c_type, _ = util.extract_ptr_type2(field_type)
#             java_type = c_type_to_java_type.get(c_type, c_type)
#             l(f"        public {java_type}[] {jni_field_name};")

#         elif is_prim_ptr(field_type) or is_struct_ptr(field_type):
#             c_type, _ = util.extract_ptr_type2(field_type)
#             jni_type = c_type_to_java_type.get(c_type, c_type)

#             # Checking if it's an array: need a variable that designates the array size
#             count_name = field_name+'Count'
#             count = get_array_count_field(decl, count_name)

#             if not count:
#                 l(f"        public {jni_type} {jni_field_name};")
#             else:
#                 l(f"        public {jni_type}[] {jni_field_name};")

#         elif util.is_1d_array_type(field_type):
#             array_type = util.extract_array_type(field_type)
#             array_sizes = util.extract_array_sizes(field_type)

#             if is_prim_ptr(array_type) or is_struct_ptr(array_type):
#                 c_type, _ = util.extract_ptr_type2(array_type)
#                 jni_type = c_type_to_java_type.get(c_type, c_type)

#                 l(f"        public {jni_type}[] {jni_field_name};")

#         elif util.is_func_ptr(field_type):
#             print(f"Function pointers aren't yet supported. Skipping {struct_name}.{field_name}")
#         #     l(f"        public {jni_field_name}: ?*const fn ({funcptr_args_c(field_type, prefix)}) callconv(.C) {funcptr_result_c(field_type)} = null,")
#         # elif util.is_1d_array_type(field_type):
#         #     array_type = util.extract_array_type(field_type)
#         #     array_sizes = util.extract_array_sizes(field_type)
#         #     if is_prim_type(array_type) or is_struct_type(array_type):
#         #         if is_prim_type(array_type):
#         #             zig_type = as_zig_prim_type(array_type)
#         #             def_val = type_default_value(array_type)
#         #         elif is_struct_type(array_type):
#         #             zig_type = as_zig_struct_type(array_type, prefix)
#         #             def_val = '.{}'
#         #         elif is_enum_type(array_type):
#         #             zig_type = as_zig_enum_type(array_type, prefix)
#         #             def_val = '.{}'
#         #         else:
#         #             sys.exit(f"ERROR gen_struct is_1d_array_type: {array_type}")
#         #         t0 = f"[{array_sizes[0]}]{zig_type}"
#         #         t1 = f"[_]{zig_type}"
#         #         l(f"        {jni_field_name}: {t0} = {t1}{{{def_val}}} ** {array_sizes[0]},")
#         #     elif util.is_const_void_ptr(array_type):
#         #         l(f"        {jni_field_name}: [{array_sizes[0]}]?*const anyopaque = [_]?*const anyopaque{{null}} ** {array_sizes[0]},")
#         #     else:
#         #         sys.exit(f"ERROR gen_struct: array {jni_field_name}: {field_type} => {array_type} [{array_sizes[0]}]")
#         # elif util.is_2d_array_type(field_type):
#         #     array_type = util.extract_array_type(field_type)
#         #     array_sizes = util.extract_array_sizes(field_type)
#         #     if is_prim_type(array_type):
#         #         zig_type = as_zig_prim_type(array_type)
#         #         def_val = type_default_value(array_type)
#         #     elif is_struct_type(array_type):
#         #         zig_type = as_zig_struct_type(array_type, prefix)
#         #         def_val = ".{}"
#         #     else:
#         #         sys.exit(f"ERROR gen_struct is_2d_array_type: {array_type}")
#         #     t0 = f"[{array_sizes[0]}][{array_sizes[1]}]{zig_type}"
#         #     l(f"        {jni_field_name}: {t0} = [_][{array_sizes[1]}]{zig_type}{{[_]{zig_type}{{{def_val}}} ** {array_sizes[1]}}} ** {array_sizes[0]},")
#         else:
#             sys.exit(f"ERROR gen_struct: {field_name}: {field_type};")
#     l("    };")

# def gen_consts(decl, prefix):
#     for item in decl['items']:
#         item_name = check_override(item['name'])
#         l(f"pub const {util.as_lower_snake_case(item_name, prefix)} = {item['value']};")

# def gen_const_var(decl, prefix):
#     item_name = check_override(decl['name'])
#     field_type = check_override(f'{prefix}.{item_name}', default=decl['type'])
#     suffix =  ''
#     if field_type == 'float':
#         suffix = 'f'
#     l(f"    public static final {field_type} {item_name} = {decl['value']}{suffix};")

# def gen_enum(decl, prefix):
#     enum_name = check_override(decl['name'])

#     has_values = any(['value' in item for item in decl['items']])

#     l(f'    public enum {enum_name} {{')

#     last_value = -1
#     count = len(decl['items'])
#     for i, item in enumerate(decl['items']):
#         lend = ','
#         if i == (count-1):
#             lend = ';'
#         item_name = check_override(item['name'])
#         if item_name != "FORCE_U32":
#             if 'value' in item:
#                 l(f"        {item_name}({item['value']})" + lend)
#                 last_value = int(item['value'])
#             else:
#                 l(f"        {item_name}({last_value+1})" + lend)
#                 last_value += 1
#     l(f'        private final int value;')
#     l(f'        private {enum_name}(int value) {{')
#     l(f'            this.value = value;')
#     l(f'        }}')
#     l(f'        public int getValue() {{')
#     l(f'            return this.value;')
#     l(f'        }}')
#     l(f'        static public {enum_name} fromValue(int value) throws IllegalArgumentException {{')
#     l(f'            for ({enum_name} e : {enum_name}.values()) {{')
#     l(f'                if (e.value == value)')
#     l(f'                    return e;')
#     l(f'            }}')
#     l(f'            throw new IllegalArgumentException(String.format("Invalid value to {enum_name}: %d", value) );')
#     l(f'        }}')

#     l("    };")
#     l("")

class State(object):
    struct_types = []
    enum_types = []
    struct_typedefs = []
    function_typedefs = []
    functions = []
    def __init__(self):
        pass

def _find_node(inp, expected, debug=False):
    if not expected:
        if debug:
            print("  list empty")
        return inp

    e = expected[0]
    if inp['kind'] != e:
        if debug:
            print("  diff:", inp['kind'], e)
        return None

    if debug:
        print("  match:", inp['kind'], e)

    inner = inp.get('inner', [])
    if not inner:
        return inp

    for i in inner:

        n = _find_node(i, expected[1:], debug)
        if debug:
            print("  inner:", i['kind'], expected[1:], n)

        if n:
            return n

    if debug:
        print("  no child found. None")
    return None

def is_struct_typedef(inp):
    # typedef struct Foo* HFoo;
    n = _find_node(inp, ['TypedefDecl', 'PointerType', 'ElaboratedType', 'RecordType'])
    # typedef struct Foo {} Foo;
    if not n:
        n = _find_node(inp, ['TypedefDecl', 'ElaboratedType', 'RecordType'])
    return n is not None

def is_function_typedef(inp):
    n = _find_node(inp, ['TypedefDecl', 'PointerType', 'ParenType', 'FunctionProtoType'])
    return n is not None

##############################################################################

class Documentation(object):
    def __init__(self, short_desc='', desc='', params=None, examples=None):
        self.short_desc = short_desc
        self.desc = desc
        self.params = params or []
        self.examples = examples or []

class Param(object):
    def __init__(self, param, name='', direction='in', text=''):
        self.param = param
        self.name = name
        self.direction = direction
        self.text = text

class Enum(object):
    def __init__(self, name):
        self.name = name
        self.values = []

# class Function(object):
#     def __init__(self, name='', args=None, ret_type='void'):
#         self.name = name
#         self.args = args or []
#         self.ret_type = ret_type

def extract_documentation(data, decl, doc):
    for child in decl.get('inner', []):
        if child['kind'] in ['FullComment']:
            r = child.get('range', {})
            begin = r.get('begin', {})
            end = r.get('end', {})
            begin_offset = begin.get('offset', -1)
            end_offset = end.get('offset', -1)

            if begin_offset > 0 and end_offset > 0:
                doc.desc = data[begin_offset:end_offset]

            return doc

    return None

def evaluate_expr(name, decl_list):
    for inner in decl_list:
        kind = inner['kind']
        if kind == 'ConstantExpr':
            return int(inner['value'])
        elif kind == 'UnaryOperator':
            return - int(evaluate_expr(name, inner['inner']))
        elif kind == 'ImplicitCastExpr':
            return evaluate_expr(name, inner['inner'])
        elif kind == 'ConstantExpr':
            return int(inner['value'])
        elif kind == 'IntegerLiteral':
            if inner['valueCategory'] not in ('rvalue', 'prvalue'):
                sys.exit(f"ERROR: Enum value ConstantExpr must be 'rvalue' or 'prvalue' ({name}), is '{inner['valueCategory']}'")
            return int(inner['value'])

def parse_enum(decl, main_prefix=None):
    outp = {}
    if 'name' in decl:
        outp['kind'] = 'enum'
        outp['name'] = decl['name']
        needs_value = False
    else:
        outp['kind'] = 'consts'
        needs_value = True
    outp['items'] = []
    for item_decl in decl['inner']:
        if item_decl['kind'] == 'EnumConstantDecl':
            item = {}
            item['name'] = item_decl['name']
            if 'inner' in item_decl:
                value = evaluate_expr(item_decl['name'], item_decl['inner'])
                if value is not None:
                    item['value'] = '%d' % value
            if needs_value and 'value' not in item:
                sys.exit(f"ERROR: anonymous enum items require an explicit value")
            outp['items'].append(item)
    return outp

def parse(data, inp, state):
    for decl in inp['inner']:
        kind = decl['kind']
        if kind == 'namespace':
            parse(data, decl, state)

        elif kind in ['struct', 'RecordDecl']:
            doc = extract_documentation(data, decl, Documentation())
            state.struct_types.append( (decl, doc) )

        elif is_struct_typedef(decl):
            doc = extract_documentation(data, decl, Documentation())
            state.struct_typedefs.append( (decl, doc) )

        elif is_function_typedef(decl):
            doc = extract_documentation(data, decl, Documentation())
            state.function_typedefs.append( (decl, doc) )

        elif kind in ['FunctionDecl']:
            doc = extract_documentation(data, decl, Documentation())
            state.functions.append( (decl, doc) )

        elif kind in ['enum', 'EnumDecl']:
            doc = extract_documentation(data, decl, Documentation())
            item = parse_enum(decl)
            enum = Enum(item['name'])
            for value in item['items']:
                enum.values.append(value)
            state.enum_types.append((enum, doc))


def cleanup_implicit(ast):
    is_implicit = ast.get('isImplicit', False)
    if is_implicit:
        return None

    storeage_class = ast.get('storageClass', '')
    if storeage_class == 'extern':
        return None
    return ast

def cleanup_files(ast, source_path):
    loc = ast.get('loc', {})
    file = loc.get('file', None)
    if not file:
        file = loc.get('includedFrom', None)

    if file:
        if not source_path in file:
            return None
    return ast

def cleanup_ast(ast, source_path):
    ast = cleanup_implicit(ast)
    if not ast:
        return None
    ast = cleanup_files(ast, source_path)
    if not ast:
        return None

    inner = []
    for sub in ast.get('inner', []):
        sub = cleanup_ast(sub, source_path)
        if sub:
            inner.append(sub)
    ast['inner'] = inner
    return ast

def parse_c_includes(path):
    r = re.compile(r"^\s*\#\s*include\s+(?:<(.+)>|\"(.+)\")") # Include reg ex
    matches = []
    with open(path) as f:
        lines = f.readlines()
        for line in lines:
            m = r.match(line)
            if m:
                matches.append(m)

    includes = [(m.group(1) or m.group(2)).strip() for m in matches]

    # Prune any includes that match the source file
    # i.e. we don't want to include the output file
    basename = os.path.basename(path)
    hppname = os.path.splitext(basename)[0] + ".hpp"
    includes = [i for i in includes if not i == hppname]
    return includes


def generate(gen_info, includes, basepath, outdir):
    for file_info in gen_info['files']:
        path = file_info['file']
        basename = os.path.splitext(os.path.basename(path))[0]
        relative_path = os.path.relpath(path, basepath)

        parsed_includes = parse_c_includes(path)
        
        data = None
        with open(path, 'r', encoding='utf-8') as f:
            data = f.read()

        ast = gen_ir.gen(path, includes)
        ast = cleanup_ast(ast, path)

        with open('debug.json', 'w') as f:
            pprint.pprint(ast, f)

        state = State()
        parse(data, ast, state)

        for lang_key in file_info["languages"].keys():
            info = dict(file_info["languages"].get(lang_key))
            info['license'] = apply_license.LICENSE

            if lang_key == "cpp":
                out = os.path.join(outdir, os.path.splitext(path)[0] + ".hpp")
                out_data = gen_cpp.gen_cpp_header(relative_path, out, info, ast, state, parsed_includes)

                with open(out, 'w', encoding='utf-8') as f:
                    f.write(out_data)
                    print("Generated", out)

def Usage(parser):
    parser.print_help()
    sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='gen_sdk',
                                     description='Generate dmSDK source from .h files')
    parser.add_argument('-i', '--input', help = "A .json file with parameters for the generation")
    parser.add_argument('-b', '--basepath', help = "Absolute base path for making includes relative")
    parser.add_argument('-o', '--output', help = "Folder where all relative files are put")
    parser.add_argument('-v', '--verbose', action='store_true')  # on/off flag

    args = parser.parse_args()
    if not args.input:
        Usage(parser)

    info = None
    with open(args.input) as f:
        info = json.loads(f.read())

    cwd = os.getcwd()

    if not args.output:
        args.output = cwd

    if not args.basepath:
        args.basepath = info.get('basepath', cwd)
    args.basepath = os.path.abspath(args.basepath)
 
    includes = info["includes"]
    includes = [os.path.expandvars(i) for i in includes]
    includes = [os.path.join(cwd, i) for i in includes]
    generate(info, includes, args.basepath, args.output)





