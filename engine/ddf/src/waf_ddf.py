import os, re
import Task, TaskGen
from TaskGen import extension, feature, after, before
import Utils

Task.simple_task_type('ddf_jar', '${JAR} ${JARCREATE} ${TGT} ${DDF_JAR_OPTIONS}',
                      color='PINK',
                      after='javac',
                      shell=False)

# TODO: Should be ddf and not '*'.
# '*' for legacy reasons
@feature('*')
@before('apply_core')
def apply_ddf(self):
    self.ddf_javaclass_dirs = set()
    self.ddf_javaclass_inputs = []

    Utils.def_attrs(self, proto_gen_cc = False,
                          proto_compile_cc = False,
                          proto_gen_java = False)

@feature('ddf')
@after('apply_core')
def apply_ddf_jar(self):
    if not hasattr(self, 'ddf_jar'):
        return

    options = []
    for d in self.ddf_javaclass_dirs:
        options.append('-C')
        options.append(d)
        options.append('.')

    tsk = self.create_task('ddf_jar')
    tsk.set_inputs(self.ddf_javaclass_inputs)
    out = self.path.find_or_declare(self.ddf_jar)
    tsk.set_outputs(out)
    tsk.env.DDF_JAR_OPTIONS = options

    if self.install_path:
        self.bld.install_files('${PREFIX}/share/java', out.abspath(self.env), self.env)

def scan_file_import(self, path):
    f = open(path, 'r')
    ret = set()
    for line in f:
        m = re.match('\s*import\s*"([^"]*?)"\s*;', line)
        if m:
            ret.add(m.groups()[0])
    return ret

def scan_file(self, file, include_nodes, scanned):
    for incn in include_nodes:
        n = incn.find_resource(file)
        if n:
            if n.abspath() in scanned:
                return []
            scanned.add(n.abspath())
            return [n] + scan_node(self, n, include_nodes, scanned)

    return []

def scan_node(self, node, include_nodes, scanned):
    deps = scan_file_import(self, node.abspath())
    ret = []
    for d in deps:
        ret += scan_file(self, d, include_nodes, scanned)
    return ret

def do_scan(self, inputs, includes):
    includes = Utils.to_list(includes)
    include_nodes = [ self.generator.path.find_dir(x) for x in includes ]
    include_nodes = filter(lambda x: x, include_nodes)
    depnodes = []
    for n in inputs:
        if n.name.endswith('.proto'):
            scanned = set()
            # NOTE: Is it really correct to return dependency on self?
            # Seems to be required
            depnodes += [n]
            depnodes += scan_node(self, n, include_nodes + [n.parent], scanned)
    return depnodes, []

def scan(self):
    includes = Utils.to_list(getattr(self.generator, 'protoc_includes', []))
    return do_scan(self, self.inputs, includes)

def configure(conf):
    conf.find_program('ddfc_sol', var='DDFC_SOL', mandatory = True)
    conf.find_program('ddfc_cxx', var='DDFC_CXX', mandatory = True)

bproto = Task.simple_task_type('bproto', 'protoc \
--plugin=protoc-gen-ddf=${DDFC_CXX} \
--ddf_out=${TGT[0].dir(env)} \
-I ${SRC[0].src_dir(env)} ${PROTOC_FLAGS} ${SRC}',
                      color='PINK',
                      before='cc cxx javac',
                      after='proto_b',
                      shell=True)
bproto.scan = scan

bproto_sol = Task.simple_task_type('bproto_sol', 'protoc \
--plugin=protoc-gen-ddf=${DDFC_SOL} \
--ddf_out=${TGT[0].dir(env)} \
-I ${SRC[0].src_dir(env)} ${PROTOC_FLAGS} ${SRC}',
                      color='PINK',
                      before='sol',
                      after='proto_b',
                      shell=True)
bproto_sol.scan = scan

def bproto_file(self, node):

    protoc = self.create_task('bproto')
    protoc.env['PROTOC_FLAGS'] = get_protoc_flags(self)
    protoc.set_inputs(node)
    out = node.change_ext('.cpp')
    h_out = node.change_ext('.h')
    protoc.set_outputs([out, h_out])

    self.allnodes.append(out)

    # TODO: Appending java_node doesn't work. Missing "extension-featre" in built-in tool?
    # An explicit node is create below

    if hasattr(self, "ddf_namespace"):
        protoc.env['ddf_options'] = '--ns %s' % self.ddf_namespace

    pt = open(node.abspath(), 'rb').read()
    pn = re.findall('\(sol_modulename\)\W+=\W+\"(.*)\";', pt)
    if len(pn) > 0:
        protosol = self.create_task('bproto_sol')
        protosol.env['PROTOC_FLAGS'] = get_protoc_flags(self)
        protosol.set_inputs(node)

        if hasattr(self, "ddf_namespace"):
            protosol.env['ddf_options'] = '--ns %s' % self.ddf_namespace

        out = node.parent.find_or_declare(pn[0] + '.sol')
        protosol.set_outputs([out])
        self.allnodes.append(out)


proto_b = Task.simple_task_type('proto_b', 'protoc -o${TGT} -I ${SRC[0].src_dir(env)} ${PROTOC_FLAGS} ${SRC}',
                                 color='PINK',
                                 before='cc cxx',
                                 shell=True)

proto_b.scan = scan

proto_gen_cc = Task.simple_task_type('proto_gen_cc', 'protoc --cpp_out=${PROTO_OUT_DIR} ${PROTOC_FLAGS} ${SRC}',
                                      color='RED',
                                      before='cc cxx',
                                      after='proto_b',
                                      shell=True)
proto_gen_cc.scan = scan

proto_gen_py = Task.simple_task_type('proto_gen_py', 'protoc --python_out=${PROTO_OUT_DIR} ${PROTOC_FLAGS} ${SRC}',
                                     color='RED',
                                     before='cc cxx',
                                     after='proto_b',
                                     shell=True)
proto_gen_py.scan = scan

proto_gen_py_package = Task.simple_task_type('proto_gen_py_package', 'echo "" > ${TGT}',
                                             color='RED',
                                             before='cc cxx',
                                             after='proto_b',
                                             shell=True)

proto_gen_java = Task.simple_task_type('proto_gen_java', 'protoc --java_out=${JAVA_OUT} ${PROTOC_FLAGS} ${SRC}',
                                       color='RED',
                                       before='cc cxx',
                                       after='proto_b',
                                       shell=True)
proto_gen_java.scan = scan

def compile_java_file(self, java_node, outdir):
    # Set variables for javac
    if self.env['CLASSPATH'] == []:
        self.env['CLASSPATH'] = ''
    self.env['CLASSPATH'] = self.env['CLASSPATH'] + os.pathsep + "." + os.pathsep + self.classpath + os.pathsep + outdir
    self.env['OUTDIR'] = outdir
    java_node_out = java_node.change_ext('.class')
    javac = self.create_task('javac')
    javac.set_inputs(java_node)
    javac.set_outputs(java_node_out)

def get_protoc_flags(self):
    protoc_includes = [self.env['DYNAMO_HOME'] + '/ext/include', self.env['DYNAMO_HOME'] + '/share/proto']
    if hasattr(self, "protoc_includes"):
        protoc_includes = Utils.to_list(self.protoc_includes) + protoc_includes

    protoc_flags = ""
    for pi in protoc_includes:
        if os.path.isabs(pi):
            p = pi
        else:
            p = self.path.find_dir(pi).srcpath(self.env)

        protoc_flags += " -I %s " % p
    return protoc_flags

def find_proto_relpath(self, node, includes):
    for pi in includes:
        if not os.path.isabs(pi):
            p = self.path.find_dir(pi).srcpath(self.env)
            rel_path = os.path.relpath(node.abspath(), self.path.find_dir(pi).abspath())
            if rel_path.find('..') != -1:
                raise Utils.WafError('Relative path: %s for file %s and include dir %s containts "..". Change or reorder include path to remove parent path references.' %
                                     (rel_path, node.abspath(), self.path.find_dir(pi).abspath()))
            return os.path.dirname(rel_path)

    raise Utils.WafError('No relative include-path path for % found' % node.abspath())

def proto_out_dir(self, node, n_parent):
    out_dir_node = node
    for i in range(max(0, n_parent)):
        out_dir_node = out_dir_node.parent
    return out_dir_node.dir(self.env)

@extension('.proto')
def proto_file(self, node):
        Utils.def_attrs(self, java_package=None, classpath='.', protoc_includes=['.'])
        bproto_file(self, node)

        task = self.create_task('proto_b')
        task.set_inputs(node)
        out = node.change_ext('.bproto')

        protoc_includes = [self.env['DYNAMO_HOME'] + '/ext/include', self.env['DYNAMO_HOME'] + '/share/proto']
        protoc_includes = Utils.to_list(self.protoc_includes) + protoc_includes

        rel_path = find_proto_relpath(self, node, Utils.to_list(self.protoc_includes))
        n_parent = 0
        tmp = rel_path
        while os.path.basename(tmp):
            n_parent += 1
            tmp = os.path.dirname(tmp)

        protoc_flags = ""
        for pi in protoc_includes:
            if os.path.isabs(pi):
                p = pi
            else:
                p = self.path.find_dir(pi).srcpath(self.env)

            protoc_flags += " -I %s " % p

        task.env['PROTOC_FLAGS'] = protoc_flags

        if self.install_path:
            self.bld.install_files('${PREFIX}/share/proto/%s' % rel_path, out.abspath(self.env), self.env)

        if self.install_path:
            # For backward compatibility
            # proto-files imported without package name shoudl
            # still install the header i APPNAME
            if not rel_path:
                tmp = Utils.g_module.APPNAME
            else:
                tmp = rel_path
            self.bld.install_files('${PREFIX}/include/%s' % tmp,
                                   out.abspath(self.env).replace(".bproto", ".h"), self.env)

        task.set_outputs(out)

        # WHY?!
        if not hasattr(self, "compiled_tasks"):
            self.compiled_tasks = []

        if hasattr(self, "proto_gen_cc") and self.proto_gen_cc:
            task = self.create_task('proto_gen_cc')
            task.env['PROTO_RELPATH'] = rel_path
            task.env['PROTOC_FLAGS'] = protoc_flags
            task.set_inputs(node)
            cc_out = node.change_ext('.pb.cc')
            h_out = node.change_ext('.pb.h')
            task.env['PROTO_OUT_DIR'] = proto_out_dir(self, cc_out, n_parent)

            task.set_outputs([cc_out, h_out])
            if hasattr(self, "proto_compile_cc") and self.proto_compile_cc:
                self.allnodes.append(cc_out)

        if hasattr(self, "proto_gen_py") and self.proto_gen_py:
            task = self.create_task('proto_gen_py')
            task.env['PROTOC_FLAGS'] = protoc_flags
            task.set_inputs(node)
            py_out = node.change_ext('_pb2.py')
            task.env['PROTO_OUT_DIR'] = proto_out_dir(self, py_out, n_parent)

            gen_py_proto_packages = getattr(self, 'gen_py_proto_packages', set())
            if rel_path and not py_out.parent.abspath() in gen_py_proto_packages:
                # Create a __init__.py package file
                pkg_task = self.create_task('proto_gen_py_package')
                pkg_out = node.parent.find_or_declare('__init__.py')
                pkg_task.set_inputs(node)
                pkg_task.set_outputs(pkg_out)
                gen_py_proto_packages.add(py_out.parent.abspath())
                if self.install_path:
                    self.bld.install_files('${PREFIX}/lib/python/%s' % rel_path, pkg_out.abspath(self.env), self.env)

                # Only create __init__.py once
                gen_py_proto_packages.add(py_out.parent.abspath())
                self.gen_py_proto_packages = gen_py_proto_packages

            if self.install_path:
                self.bld.install_files('${PREFIX}/lib/python/%s' % rel_path, py_out.abspath(self.env), self.env)
            task.set_outputs(py_out)

        if hasattr(self, "proto_gen_java") and self.proto_gen_java:
            task = self.create_task('proto_gen_java')
            task.env['PROTOC_FLAGS'] = protoc_flags
            task.set_inputs(node)

            proto_text = open(node.abspath(), 'rb').read()
            package_name = re.findall('\W*option\W+java_package\W*=\W*"(.*)"', proto_text)
            java_outer_classname = re.findall('\W*option\W+java_outer_classname\W*=\W*"(.*)"', proto_text)

            if not package_name:
                raise Utils.WafError("No 'package_name' found in: %s" % node.abspath())

            if not java_outer_classname:
                raise Utils.WafError("No 'java_outer_classname' found in: %s" % node.abspath())

            package_name = package_name[0]
            java_outer_classname = java_outer_classname[0]

            package_dir = package_name.replace('.', '/')
            out.proto_java_classname = java_outer_classname

            outdir = os.path.dirname(out.abspath(self.env))
            java_out_dir = '%s/generated' % outdir
            task.env['JAVA_OUT'] = '%s/generated' % outdir
            if not os.path.exists(java_out_dir):
                os.makedirs(java_out_dir)

            java_out = node.parent.exclusive_build_node('generated/%s/%s.java' % (package_dir, out.proto_java_classname))
            task.set_outputs(java_out)

            self.ddf_javaclass_dirs.add(node.parent.find_dir('generated').abspath(self.env))
            self.ddf_javaclass_inputs.append(java_out.change_ext('.class'))

            compile_java_file(self, java_out, java_out_dir)

