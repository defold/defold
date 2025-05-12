#-------------------------------------------------------------------------------
#   Generate an intermediate representation of a clang AST dump.
#-------------------------------------------------------------------------------
import os, sys, json, subprocess
from log import log

def is_api_decl(decl, prefix):
    if 'type' in decl and decl['type']['qualType'].startswith(prefix):
        return True
    if 'name' in decl and decl['name'].startswith(prefix):
        return True
    elif decl['kind'] == 'EnumDecl':
        # an anonymous enum, check if the items start with the prefix
        return decl['inner'][0]['name'].lower().startswith(prefix)
    else:
        return False

def is_dep_decl(decl, dep_prefixes):
    for prefix in dep_prefixes:
        if is_api_decl(decl, prefix):
            return True
    return False

def dep_prefix(decl, dep_prefixes):
    for prefix in dep_prefixes:
        if is_api_decl(decl, prefix):
            return prefix
    return None

def filter_types(str):
    return str.replace('_Bool', 'bool')


def parse_struct(decl):
    outp = {}
    outp['kind'] = 'struct'
    outp['name'] = decl['name']
    outp['fields'] = []
    for item_decl in decl['inner']:
        if item_decl['kind'] != 'FieldDecl':
            printtable(item_decl)
            sys.exit(f"ERROR: Structs must only contain simple fields ({decl['name']})")
        item = {}
        if 'name' in item_decl:
            item['name'] = item_decl['name']
        item['type'] = filter_types(item_decl['type']['qualType'])
        outp['fields'].append(item)
    return outp

def parse_cpp_struct(decl):
    if 'inner' not in decl:
        return None # Skip forward declaration

    outp = {}
    outp['kind'] = decl['tagUsed']
    outp['name'] = decl['name']
    outp['fields'] = []
    for item_decl in decl['inner']:
        if item_decl['kind'] in ('CXXConstructorDecl', 'CXXDestructorDecl', 'CXXRecordDecl', 'CXXMethodDecl'):
            continue

        if item_decl['kind'] in ('MaxFieldAlignmentAttr', 'AlignedAttr'):
            sys.exit(f"ERROR: Alignment is not yet supported ({decl['name']} - {item_decl['kind']})")
            continue

        if item_decl['kind'] not in ('FieldDecl',):
            printtable(item_decl)
            sys.exit(f"ERROR: Structs must only contain simple fields ({decl['name']} - {item_decl['kind']})")
        item = {}
        if 'name' in item_decl:
            item['name'] = item_decl['name']
        item['type'] = filter_types(item_decl['type']['qualType'])
        outp['fields'].append(item)
    return outp

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
            if main_prefix is not None and not is_api_decl(item_decl, main_prefix):
                print("Skipping enum", decl.get('name', 'anonymous'))
                return None
            item = {}
            item['name'] = item_decl['name']
            if 'inner' in item_decl:
                value = evaluate_expr(item_decl['name'], item_decl['inner'])
                item['value'] = '%d' % value
            if needs_value and 'value' not in item:
                sys.exit(f"ERROR: anonymous enum items require an explicit value")
            outp['items'].append(item)
    return outp

def parse_func(decl):
    outp = {}
    outp['kind'] = 'func'
    outp['name'] = decl['name']
    outp['type'] = filter_types(decl['type']['qualType'])
    outp['params'] = []
    if 'inner' in decl:
        for param in decl['inner']:
            if param['kind'] != 'ParmVarDecl':
                print(f"  >> warning: ignoring func {decl['name']} (unsupported parameter type {param['kind']})")
                continue
            outp_param = {}
            outp_param['name'] = param['name']
            outp_param['type'] = filter_types(param['type']['qualType'])
            outp['params'].append(outp_param)
    return outp

def parse_namespace(decl):
    outp = {}
    outp['kind'] = 'namespace'
    outp['name'] = decl['name']
    return outp


def get_constant(decl):
    if 'VarDecl' != decl['kind']:
        return None
    child = decl['inner'][0]
    if child['kind'] == 'ImplicitCastExpr':
        child = child['inner']
    if child['kind'] in ('IntegerLiteral', 'FloatingLiteral'):
        return child['value']
    return None

def parse_var(decl):
    outp = {}
    outp['kind'] = 'var'
    outp['name'] = decl['name']

    value = get_constant(decl)
    if value is None:
        sys.exit(f"ERROR: VarDecl must have a IntegerLiteral or IntegerLiteral: ({decl['name']})")

    var_decl = decl['inner'][0]
    outp['type'] = filter_types(var_decl['type']['qualType'])
    outp['value'] = var_decl['value']
    return outp

def parse_decl(decl):
    kind = decl['kind']
    if kind == 'RecordDecl':
        return parse_struct(decl)
    elif kind == 'CXXRecordDecl':
        return parse_cpp_struct(decl)
    elif kind == 'EnumDecl':
        return parse_enum(decl)
    elif kind == 'FunctionDecl':
        return parse_func(decl)
    elif kind == 'NamespaceDecl':
        return parse_namespace(decl)
    elif kind == 'VarDecl':
        return parse_var(decl)
    else:
        return None

def get_type(decl):
    if not 'type' in decl:
        return "unknown"
    return decl['type']['qualType']

def printtable(t):
    s = json.dumps(t, sort_keys=True, indent=2)
    print(s)

def parse_inner_cpp(main_prefix, dep_prefixes, namespace, inp, outp):
    if not 'decls' in outp:
        outp['decls'] = []
    for decl in inp['inner']:
        is_dep = is_dep_decl(decl, dep_prefixes)

        if 'NamespaceDecl' == decl['kind']:
            if not (is_api_decl(decl, main_prefix) or is_dep):
                continue
            outp_decl = parse_namespace(decl)
            parse_inner_cpp(main_prefix, dep_prefixes, outp_decl['name'], decl, outp_decl)
            outp['decls'].append(outp_decl)

        elif is_api_decl(decl, main_prefix) or is_dep or namespace:
            outp_decl = parse_decl(decl)
            if outp_decl is not None:
                outp_decl['is_dep'] = is_dep
                outp_decl['dep_prefix'] = dep_prefix(decl, dep_prefixes)
                outp['decls'].append(outp_decl)


def clang(csrc_path, includes=[]):
    clang = os.environ.get('CLANG', 'clang')
    cmd = [clang, '-Xclang', '-ast-dump=json', '-c']
    cmd.extend([ '-I%s' % include for include in includes])
    cmd.append(csrc_path)
    log('[exec] %s' % cmd)
    return subprocess.check_output(cmd)

def clang_cpp(csrc_path, includes=[]):
    clangpp = os.environ.get('CLANGPP', 'clang++')
    cmd = [clangpp, '-Xclang', '-ast-dump=json', '-c']
    cmd.extend([ '-I%s' % include for include in includes])
    cmd.append(csrc_path)
    log('[exec] %s' % cmd)
    return subprocess.check_output(cmd)

def gen(source_path, includes, module, main_prefix, dep_prefixes):
    ast = clang_cpp(source_path, includes)
    inp = json.loads(ast)
    # with open(f'{module}.src.json', 'w') as f:
    #     f.write(json.dumps(inp, indent=2));

    outp = {}
    outp['module'] = module
    outp['prefix'] = main_prefix
    outp['dep_prefixes'] = dep_prefixes
    outp['decls'] = []
    if os.path.splitext(source_path)[1] in ('.cc', '.cpp', '.cxx'):
        parse_inner_cpp(main_prefix, dep_prefixes, None, inp, outp)

    # with open(f'{module}.json', 'w') as f:
    #     f.write(json.dumps(outp, indent=2));
    return outp
