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


# SOme good-to-read references

# examples: https://stackoverflow.com/questions/11968960/how-use-pinvoke-for-c-struct-array-pointer-to-c-sharp
# field offset: https://stackoverflow.com/questions/8757855/getting-pointer-to-struct-inside-itself-unsafe-context



import os, sys, re, pprint

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
    'const char *': 'String',
    'const char *const *': 'String*',
    'const void *': 'void*',
    'int32_t':      'int',
    'uint32_t':     'uint',
    'int16_t':      'short',
    'uint16_t':     'ushort',
    'int8_t':       'char',
    'uint8_t':      'byte',
    'dmhash_t':     'UInt64',
    'uint64_t':     'UInt64'
}

# in order to avoid reserved keywords
c_to_csvars = {
    'string': 'str',
    'ref': '_ref'
}

def rename_symbols(renames, line):
    for k, v in renames.items():
        line = line.replace(k, v)
    return line

def gen_doc_header(name, brief, description, language, namespace, source):
    return f"""
/*# {brief}
 *
 * {description}
 *
 * @document
 * @language {language}
 * @name {name}
 * @namespace {namespace}
 * @path {source}
 */
"""

def get_indent(indent):
    return '    ' * indent

def _find_doc(docs, name):
    for doc in docs['items']:
        if doc.get('name', '') == name:
            return doc
    return None

def _add_line(lines, tag, s):
    if s is None:
        return
    if not tag:
        lines.append(s)
    else:
        lines.append(f'@{tag} {s}')

def _add_param(lines, tag, doc):
    if doc is None:
        return
    name = doc.get('name', '')
    typ = doc.get('type', '')
    cstyp = rename_type(typ)
    desc = doc.get('desc', '')
    lines.append(f'<{tag} name="{name}" type="{cstyp}">{desc}</{tag}>')

def _format_doc_lines(lines, indent):
    indent_str =  get_indent(indent)
    if isinstance(lines, str):
        lines = lines.splitlines()
    return ('\n' + indent_str + '/// ').join(lines)

def gen_doc(docs, name, indent):
    indent_str = get_indent(indent)
    doc = _find_doc(docs, name)
    if not doc:
        s  = f'{indent_str}/// <summary>\n'
        s += f'{indent_str}/// Generated from [ref:{name}]\n'
        s += f'{indent_str}/// </summary>'
        return s

    lines = []
    _add_line(lines, None, '<summary>')
    desc = doc.get('desc', doc.get('brief', ''))
    desc = _format_doc_lines(desc, indent)
    _add_line(lines, None, desc)
    _add_line(lines, None, '</summary>')

    for param in doc.get('params', []):
        _add_param(lines, 'param', param)

    _add_param(lines, 'returns', doc.get('return', None))

    for (_, v) in doc.get('examples', []):
        # TODO: Filter out languages that aren't C#
        v = _format_doc_lines('\n' + v, indent)
        _add_line(lines, 'examples', v)

    indent_str = get_indent(indent)
    return indent_str + '/// ' + _format_doc_lines(lines, indent)


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

def gen_cs_struct(decl, rename_patterns, opaque, indent):
    #pprint.pprint(decl, sys.stdout)

    name = rename_symbols(rename_patterns, decl['name'])
    while name.endswith('*'):
        name = name[:-1]

    spaces = get_indent(indent)
    spaces1 = get_indent(indent+1)
    s = ''
    s += f'{spaces}[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack=1)]\n'
    s += f'{spaces}public struct {name}\n'
    s += f'{spaces}{{\n'

    if opaque:
        s += f'{spaces}    // opaque struct\n'
    else:
        for member in decl['inner']:
            n = member['name']
            if n.startswith('m_'):
                n = n[2:]
            n = rename_symbols(rename_patterns, n)
            t = rename_symbols(rename_patterns, member['type']['qualType'])

            if t.startswith('//'):
                ot = member['type']['qualType']
                print(f"CS: Warning: exiting struct {name}, as member {ot} {n} isn't supported yet")
                break # if we skip one, we can't guarantuee the order/layout

            # TODO: Find out the layout!
            s += f'{spaces1}public {t} {n};\n'


    s += f'{spaces}}}'
    return s



def gen_csharp(basepath, c_header_path, out_path, info, ast, state, docs):
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
    using       = info.get('using', [])
    using_static= info.get('using_static', [])

    if not namespace:
        raise Exception("C# file has no 'namespace' defined!")
    if not classname:
        raise Exception("C# file has no 'classname' defined!")
    if not dllimport:
        raise Exception("C# file has no 'dllimport' defined!")

    indent = 1
    spaces = '    ' * indent
    spaces1 = '    ' * (indent+1)

    # Dock header
    api_name = docs.get('name', None)
    if api_name is None:
        api_name = os.path.splitext(os.path.basename(out_path))[0].capitalize()

    language = 'C#'
    brief = docs.get('brief', '')
    description = docs.get('desc', '')
    description = description.replace('\n', '\n * ')

    if api_name is None:
        api_name = os.path.splitext(os.path.basename(out_path))[0].capitalize()

    namespace = info.get('namespace', None)

    source_path = os.path.relpath(out_path, basepath)

    doc_header = gen_doc_header(api_name, brief, description, language, namespace, source_path)
    l(doc_header)

    # C# begin
    l(f'using System.Runtime.InteropServices;')
    l(f'using System.Reflection.Emit;')

    l(f'')
    for u in using:
        l(f'using {u};')
    for u in using_static:
        l(f'using static {u};')

    l(f'')
    l(f'namespace dmSDK.{namespace} {{')
    l(f'{spaces}public unsafe partial class {classname}')
    l(f'{spaces}{{')

    ignores = info.get('ignore', [])
    prefixes = info.get('rename', {})

    # for enum_type, doc in state.enum_types:
    #     n = enum_type.name
    #     if n in ignores:
    #         continue
    #     renamed = rename_symbols(prefixes, n)

    #     # out_doc = get_cpp_doc(prefixes, doc)
    #     # if out_doc is None:
    #     out_doc = gen_doc(docs, enum_type.name)
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

    for decl, doc in state.struct_types:
        n = decl['name']
        if n in ignores:
            continue

        struct_typedef = gen_cs_struct(decl, prefixes, False, indent+1)

        out_doc = gen_doc(docs, n, indent+1)
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
    #     out_doc = gen_doc(docs, decl['name'])
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
        out_doc = gen_doc(docs, decl['name'], indent+1)
        l(out_doc)
        l(f'{spaces1}{cs_func_prefix}')
        l(f'{spaces1}{cs_func};')
        l('')

    #print("struct_typedefs"); pprint.pp(state.struct_typedefs)
    #print("function_typedefs"); pprint.pp(state.function_typedefs)
    #print("struct_types"); pprint.pp(state.struct_types)
    #print("enum_types"); pprint.pp(state.enum_types)
    #print("functions"); pprint.pp(state.functions)

    l(f'{spaces}}} // {classname}')
    l(f'}} // dmSDK.{namespace}')

    l('') # always have a newline at the end
    return out_lines

