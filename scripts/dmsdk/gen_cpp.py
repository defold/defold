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
    desc = doc.get('desc', '')
    if not typ:
        lines.append(f'@{tag} {name} {desc}')
    else:
        lines.append(f'@{tag} {name} [type:{typ}] {desc}')

def _format_doc_lines(lines, indent):
    indent_str = get_indent(indent)
    if isinstance(lines, str):
        lines = lines.splitlines()
    return ('\n' + indent_str + ' * ').join(lines)

def gen_doc(docs, name, renamed, indent=1, language='C++'):
    doc = _find_doc(docs, name)
    if not doc:
        return f"""    /*#
        * Generated from [ref:{name}]
        */"""

    lines = []
    _add_line(lines, None, doc.get('brief', ''))

    desc = doc.get('desc', doc.get('brief', ''))
    desc = _format_doc_lines(desc, indent)
    _add_line(lines, None, desc)

    doctype = doc.get('type', None)
    if doctype:
        lines.append(f'@{doctype}')
    else:
        doctype = 'function'

    _add_line(lines, 'name', renamed)
    _add_line(lines, 'language', language)

    if doctype in ['function', 'typedef']:
        for param in doc.get('params', []):
            _add_param(lines, 'param', param)

        _add_param(lines, 'return', doc.get('return', None))

    elif doctype in ['struct', 'enum']:
        for member in doc.get('members', []):
            _add_param(lines, 'member', member)

    for (_, v) in doc.get('notes', []):
        v = _format_doc_lines(v, indent)
        _add_line(lines, 'note', v)

    for (_, v) in doc.get('examples', []):
        # TODO: Filter out languages that aren't C++
        if 'objective-c' in v:
            v = v.replace('\\@', '@')

        v = _format_doc_lines('\n' + v, indent)
        _add_line(lines, 'examples', v)

    indent_str = get_indent(indent)
    return indent_str + '/*# ' + _format_doc_lines(lines, indent) + '\n'+ indent_str + ' */'

def get_cpp_func(decl):
    ret_type = decl['type']['qualType']
    index = ret_type.find('(')
    ret_type = ret_type[:index].strip()
    
    args = []
    for inner in decl.get('inner', []):
        #args.append(inner['type']['qualType'])
        if not inner['kind'] == 'ParmVarDecl':
            continue
        args.append('%s %s' % (inner['type']['qualType'], inner.get('name','')))

    return '%s %s(%s)' % (ret_type, decl['name'], ', '.join(args))

def add_c_docs(docs):
    for doc in docs['items']:
        if not 'name' in doc:
            import pprint
            pprint.pp(doc)
        name = doc['name']
        out = gen_doc(docs, name, name, indent=0, language='C')
        l(out)
        l('')

def gen_cpp_header(basepath, c_header_path, out_path, info, ast, state, includes, docs):
    _reset_globals()

    l('// Generated, do not edit!')
    l('// Generated with cwd=%s and cmd=%s\n' % (os.getcwd(), ' '.join(sys.argv)))

    license = info.get('license', 'License: Defold License (https://github.com/defold/defold/blob/dev/LICENSE.txt)')
    license = license.split('\n')
    license = '\n'.join(['// ' + line for line in license])

    l('%s\n' % license)

    basename = os.path.basename(out_path).replace('.', '_')
    basename = basename.upper()
    l('#ifndef DMSDK_%s' % basename)
    l('#define DMSDK_%s' % basename)
    l('')
    l('#if !defined(__cplusplus)')
    l('   #error "This file is supported in C++ only!"')
    l('#endif')
    l('')

    api_name = docs.get('name', None)
    if api_name is None:
        api_name = os.path.splitext(os.path.basename(out_path))[0].capitalize()

    language = 'C++'
    brief = docs.get('brief', '')
    description = docs.get('desc', '')
    description = description.replace('\n', '\n * ')

    if api_name is None:
        api_name = os.path.splitext(os.path.basename(out_path))[0].capitalize()

    namespace = info.get('namespace', None)

    source_path = os.path.relpath(out_path, basepath)
    doc_header = gen_doc_header(api_name, brief, description, language, namespace, source_path)
    l(doc_header)

    for include in includes:
        if os.path.normpath(out_path).endswith(os.path.normpath(include)):
            continue # skip self includes
        l('#include <%s>' % include)

    l('')
    l('#include <%s>' % c_header_path)
    l('')

    if not namespace:
        raise Exception("C++ header has no 'namespace' defined!")

    l('namespace %s' % info['namespace'])
    l('{')

    ignores = info.get('ignore', [])
    prefixes = info.get('rename', {})

    for enum_type, doc in state.enum_types:
        n = enum_type.name
        if n in ignores:
            continue
        renamed = rename_symbols(prefixes, n)

        out_doc = gen_doc(docs, enum_type.name, renamed)
        l(out_doc)
        l('    enum %s {' % renamed)
        for enum_value in enum_type.values:
            n = enum_value['name']
            renamed = rename_symbols(prefixes, n)
            if 'value' in enum_value:
                l('        %s = %s,' % (renamed, enum_value['value']))
            else:
                l('        %s,' % renamed)
        l('    };')
        l('')

    for decl, doc in state.struct_typedefs:
        n = decl['name']
        if n in ignores:
            continue
        renamed = rename_symbols(prefixes, n)

        # out_doc = get_cpp_doc(prefixes, doc)
        # if out_doc is None:
        out_doc = gen_doc(docs, decl['name'], renamed)
        l(out_doc)
        l('    typedef %s %s;' % (n, renamed))
        l('')

    for decl, doc in state.function_typedefs:
        n = decl['name']
        if n in ignores:
            continue
        renamed = rename_symbols(prefixes, n)

        # out_doc = get_cpp_doc(prefixes, doc)
        # if out_doc is None:
        out_doc = gen_doc(docs, decl['name'], renamed)
        l(out_doc)
        l('    typedef %s %s;' % (n, renamed))
        l('')

    for decl, doc in state.functions:
        n = decl['name']
        if n in ignores:
            continue
        renamed = rename_symbols(prefixes, n)
        cpp_func = get_cpp_func(decl)
        cpp_func = rename_symbols(prefixes, cpp_func)

        # out_doc = get_cpp_doc(prefixes, doc)
        # if out_doc is None:
        out_doc = gen_doc(docs, decl['name'], renamed)
        l(out_doc)
        l('    %s;' % cpp_func)
        l('')

    l('')
    l('} // namespace %s' % info['namespace'])

    #print("struct_typedefs"); pprint.pp(state.struct_typedefs)
    #print("function_typedefs"); pprint.pp(state.function_typedefs)
    #print("struct_types"); pprint.pp(state.struct_types)
    #print("enum_types"); pprint.pp(state.enum_types)
    #print("functions"); pprint.pp(state.functions)

    l('')
    l('#endif // #define DMSDK_' + basename.upper())

    # Workaround, until we have proper multi language support.
    # We attach the C documentation at the end of this file.
    add_c_docs(docs)

    l('') # always have a newline at the end
    return out_lines

