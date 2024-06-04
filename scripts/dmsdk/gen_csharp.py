# Copyright 2020-2024 The Defold Foundation
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

import os, sys, pprint

###########################################
# state while parsing
out_lines = ''
last_comment = ''

###########################################
# helper functions

def _reset_globals():
    global out_lines
    out_lines = ''

def l(s):
    global out_lines
    out_lines += s + '\n'

###########################################

c_to_cstypes = {
    'const char *': 'string',
    'const void *': 'void*',
    'int32_t':      'int',
    'uint32_t':     'UInt32',
    'dmhash_t':     'UInt64',
    'uint64_t':     'UInt64',
}

c_to_csvars = {
    'string': 'str',
}

def rename_symbols(renames, line):
    for k, v in renames.items():
        line = line.replace(k, v)
    return line

def gen_doc_header(name, description, namespace, source):
    return f"""
/*# {name}
 *
 * {description}
 *
 * @document
 * @name {name}
 * @namespace {namespace}
 * @path {source}
 */
"""

def gen_doc_link(name):
    return f"""            /*#
            * Generated from [ref:{name}]
            */"""

def rename_type(name):
    return c_to_cstypes.get(name, name)

def rename_var(var):
    return c_to_csvars.get(var, var)


def gen_csharp_func_prefix(decl, dllimport):
    entrypoint = decl['name']
    calling_convention = 'CallingConvention.Cdecl'
    return f'[DllImport("{dllimport}", EntryPoint="{entrypoint}", CallingConvention = {calling_convention})]'

def gen_csharp_func(decl):
    ret_type = decl['type']['qualType']
    index = ret_type.find('(')
    ret_type = rename_type(ret_type[:index].strip())

    args = []
    for inner in decl.get('inner', []):
        #args.append(inner['type']['qualType'])
        if not inner['kind'] == 'ParmVarDecl':
            continue
        args.append('%s %s' % (rename_type(inner['type']['qualType']), rename_var(inner.get('name','')) ))

    return 'public static extern %s %s(%s)' % (ret_type, decl['name'], ', '.join(args))

def gen_cs_struct_typedef(name, indent):
    spaces = '    ' * indent
    return f'''{spaces}[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack=1)]
{spaces}public struct {name}
{spaces}{{
{spaces}    // opaque struct
{spaces}}}'''


def gen_csharp(basepath, c_header_path, out_path, info, ast, state):
    _reset_globals()

    l('// Generated, do not edit!')
    l('// Generated with cwd=%s and cmd=%s\n' % (os.getcwd(), ' '.join(sys.argv)))

    license = info.get('license', 'License: Defold License (https://github.com/defold/defold/blob/dev/LICENSE.txt)')
    license = license.split('\n')
    license = '\n'.join(['// ' + line for line in license])

    l('%s\n' % license)

    namespace = info.get('namespace', None)
    classname = info.get('class', None)
    dllimport = info.get('dllimport', None)

    if not namespace:
        raise Exception("C# file has no 'namespace' defined!")
    if not classname:
        raise Exception("C# file has no 'classname' defined!")
    if not dllimport:
        raise Exception("C# file has no 'dllimport' defined!")

    l(f'using System.Runtime.InteropServices;')
    l(f'using System.Reflection.Emit;')
    l(f'')
    l(f'namespace dmSDK {{')
    l(f'    namespace {namespace} {{')
    l(f'        public unsafe partial class {classname}')
    l(f'        {{')


    # basename = os.path.basename(out_path).replace('.', '_')
    # basename = basename.upper()
    # l('#ifndef DMSDK_%s' % basename)
    # l('#define DMSDK_%s' % basename)
    # l('')
    # l('#if !defined(__cplusplus)')
    # l('   #error "This file is supported in C++ only!"')
    # l('#endif')
    # l('')

    # namespace = info.get('namespace', None)
    # source_path = os.path.relpath(out_path, basepath)
    # doc_header = gen_doc_header(os.path.splitext(os.path.basename(out_path))[0].capitalize(), "description", namespace, source_path)
    # l(doc_header)

    # # for include in includes:
    # #     if os.path.normpath(out_path).endswith(os.path.normpath(include)):
    # #         continue # skip self includes
    # #     l('#include <%s>' % include)

    # l('')
    # l('#include <%s>' % c_header_path)
    # l('')

    ignores = info.get('ignore', [])
    prefixes = info.get('rename', {})

    # for enum_type, doc in state.enum_types:
    #     n = enum_type.name
    #     if n in ignores:
    #         continue
    #     renamed = rename_symbols(prefixes, n)

    #     # out_doc = get_cpp_doc(prefixes, doc)
    #     # if out_doc is None:
    #     out_doc = gen_doc_link(enum_type.name)
    #     l(out_doc)
    #     l('    enum %s {' % renamed)
    #     for enum_value in enum_type.values:
    #         n = enum_value['name']
    #         renamed = rename_symbols(prefixes, n)
    #         if 'value' in enum_value:
    #             l('        %s = %s,' % (renamed, enum_value['value']))
    #         else:
    #             l('        %s,' % renamed)
    #     l('    };')
    #     l('')

    for decl, doc in state.struct_typedefs:
        n = decl['name']
        if n in ignores:
            continue
        renamed = rename_symbols(prefixes, n)
        while renamed.endswith('*'):
            renamed = renamed[:-1]
        struct_typedef = gen_cs_struct_typedef(renamed, 3)

        # out_doc = get_cpp_doc(prefixes, doc)
        # if out_doc is None:
        out_doc = gen_doc_link(n)
        l(out_doc)
        l(f'{struct_typedef}')
        l('')

    # for decl, doc in state.function_typedefs:
    #     n = decl['name']
    #     if n in ignores:
    #         continue
    #     renamed = rename_symbols(prefixes, n)

    #     # out_doc = get_cpp_doc(prefixes, doc)
    #     # if out_doc is None:
    #     out_doc = gen_doc_link(decl['name'])
    #     l(out_doc)
    #     l('    typedef %s %s;' % (n, renamed))
    #     l('')


    for decl, doc in state.functions:
        n = decl['name']
        if n in ignores:
            continue

        cs_func_prefix = gen_csharp_func_prefix(decl, dllimport)
        cs_func = gen_csharp_func(decl)
        cs_func = rename_symbols(prefixes, cs_func)

        # out_doc = get_cpp_doc(prefixes, doc)
        # if out_doc is None:
        out_doc = gen_doc_link(decl['name'])
        l(out_doc)
        l(f'            {cs_func_prefix}')
        l(f'            {cs_func};')
        l('')

    # l('')

    #print("struct_typedefs"); pprint.pp(state.struct_typedefs)
    #print("function_typedefs"); pprint.pp(state.function_typedefs)
    #print("struct_types"); pprint.pp(state.struct_types)
    #print("enum_types"); pprint.pp(state.enum_types)
    #print("functions"); pprint.pp(state.functions)

    l(f'        }} // {classname}')
    l(f'    }} // {namespace}')
    l(f'}} // dmSDK')

    l('') # always have a newline at the end
    return out_lines

