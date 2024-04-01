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
    out_lines = ''

def l(s):
    global out_lines
    out_lines += s + '\n'



###########################################

def rename_symbols(renames, line):
    for k, v in renames.items():
        line = line.replace(k, v)
    return line

def gen_cpp_doc(renames, doc):
    if doc is None or not (doc.desc or doc.params or doc.examples):
        return '    // no documentation found'
    lines = []
    lines.extend(doc.desc.strip().split('\n'))
    for param in doc.params:
        if param.name:
            lines.append('@%s %s %s' % (param.param, param.name, param.text)) # @param text
        else:
            lines.append('@%s %s' % (param.param, param.text)) # @name text

    lines = [l.strip() for l in lines] # fix some annoying white spaces at the beginning of some lines

    lines = [rename_symbols(renames, l) for l in lines]

    for example in doc.examples:
        example = example.desc.strip()
        if not example.startswith("```cpp"):
            continue
        lines.append("@examples")
        lines.extend(example.split('\n'))

    if not lines:
        return '    // no documentation generated'

    lines = [lines[0]] + ['     ' + l for l in lines[1:]]
    return '    /*%s\n' % lines[0] + '\n'.join(lines[1:]) + '\n     */'

def get_cpp_func(decl):
    ret_type = decl['type']['qualType']
    index = ret_type.find('(')
    ret_type = ret_type[:index].strip()
    
    args = []
    for inner in decl.get('inner', []):
        #args.append(inner['type']['qualType'])
        if not inner['kind'] == 'ParmVarDecl':
            continue
        args.append('%s %s' % (inner['type']['qualType'], inner['name']))

    return '%s %s(%s)' % (ret_type, decl['name'], ','.join(args))

def gen_cpp_header(c_header_path, out_path, info, ast, state, includes):
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

    for include in includes:
        l('#include <%s>' % include)

    l('')
    l('#include <%s>' % c_header_path)
    l('')

    namespace = info.get('namespace', None)
    if not namespace:
        raise Exception("C++ header has no 'namespace' defined!")

    l('namespace %s {' % info['namespace'])

    ignores = info.get('ignore', [])
    prefixes = info.get('prefix', {})

    for enum_type, doc in state.enum_types:
        n = enum_type.name
        if n in ignores:
            continue
        renamed = rename_symbols(prefixes, n)
        l('%s' % gen_cpp_doc(prefixes, doc))
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
        l('%s' % gen_cpp_doc(prefixes, doc))
        l('    typedef %s %s;' % (n, renamed))
        l('')

    for decl, doc in state.function_typedefs:
        n = decl['name']
        if n in ignores:
            continue
        renamed = rename_symbols(prefixes, n)
        l('%s' % gen_cpp_doc(prefixes, doc))
        l('    typedef %s %s;' % (n, renamed))
        l('')

    for decl, doc in state.functions:
        n = decl['name']
        if n in ignores:
            continue
        renamed = rename_symbols(prefixes, n)
        cpp_func = get_cpp_func(decl)
        cpp_func = rename_symbols(prefixes, cpp_func)
        l('%s' % gen_cpp_doc(prefixes, doc))
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

    l('') # always have a newline at the end
    return out_lines

