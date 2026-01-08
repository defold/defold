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

import argparse
import os, sys, json, pprint, re, collections

# The parsing idea is based on Sokol's code generation: https://github.com/floooh/sokol/tree/master/bindgen
import gen_ir
import gen_util as util
import gen_cpp
import gen_csharp
import gen_doc

# until we have a DEFOLD_HOME
if not 'DEFOLD_HOME' in os.environ:
    os.environ['DEFOLD_HOME'] = os.path.normpath(os.path.join(os.environ["DYNAMO_HOME"], '../..'))
sys.path.insert(0, os.environ['DEFOLD_HOME'])
import apply_license

class State(object):
    def __init__(self):
        self.struct_types = []
        self.enum_types = []
        self.struct_typedefs = []
        self.function_typedefs = []
        self.functions = []


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

# Deprecated. We now use gen_doc.py for documentation extraction
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

def filter_types(str):
    return str.replace('_Bool', 'bool')

def cleanup_ast(ast, source_path):
    ast = cleanup_implicit(ast)
    if not ast:
        return None
    ast = cleanup_files(ast, source_path)
    if not ast:
        return None

    if 'type' in ast:
        ast['type']['qualType'] = filter_types(ast['type']['qualType'])

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

def prune_includes(includes):
    out = []
    for x in includes:
        # Let's remove C includes for now
        if x in ['dmsdk/lua/lua.h',
                 'dmsdk/lua/lauxlib.h',
                 'stdbool.h']:
                 continue
        out.append(x)
    return out

def generate(gen_info, includes, basepath, outdir):
    for file_info in gen_info['files']:
        path = file_info['file']
        basename = os.path.splitext(os.path.basename(path))[0]
        relative_path = os.path.relpath(path, basepath)

        parsed_includes = parse_c_includes(path)

        parsed_docs = gen_doc.parse_docs(path)

        data = None
        with open(path, 'r', encoding='utf-8') as f:
            data = f.read()

        ast = gen_ir.gen(path, includes)
        ast = cleanup_ast(ast, path)

        # with open('debug.json', 'w') as f:
        #     pprint.pprint(ast, f)

        state = State()
        parse(data, ast, state)

        languages = file_info["languages"]
        for lang_key in languages.keys():
            info = dict(file_info["languages"].get(lang_key))
            info['license'] = apply_license.LICENSE

            out_data = None

            out_path = info.get('outpath', None)

            if lang_key == "cpp":
                out = out_path or os.path.join(outdir, os.path.splitext(path)[0] + "_gen.hpp")

                parsed_includes = prune_includes(parsed_includes)

                out_data = gen_cpp.gen_cpp_header(os.environ['DEFOLD_HOME'], relative_path, out, info, ast, state, parsed_includes, parsed_docs)

            elif lang_key == "csharp":
                out = out_path or os.path.join(basepath, 'cs', os.path.splitext(relative_path)[0] + "_gen.cs")
                out_data = gen_csharp.gen_csharp(os.environ['DEFOLD_HOME'], relative_path, out, info, ast, state, parsed_docs)

            else:
                print("Unsupported language", lang_key)
                return False

            if not out_data:
                print("Failed to generate file", out)
                return False

            with open(out, 'w', encoding='utf-8') as f:
                f.write(out_data)
                print("Generated", out)

    return True

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
        info = json.JSONDecoder(object_pairs_hook=collections.OrderedDict).decode(f.read())

    cwd = os.getcwd()

    if not args.output:
        args.output = cwd

    if not args.basepath:
        args.basepath = info.get('basepath', cwd)
    args.basepath = os.path.abspath(args.basepath)
 
    includes = info["includes"]
    includes = [os.path.expandvars(i) for i in includes]
    includes = [os.path.join(cwd, i) for i in includes]
    result = generate(info, includes, args.basepath, args.output)

    if not result:
        print("Generation failed for", args.input)
        sys.exit(1)





