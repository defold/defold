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

def configure(conf):
    conf.find_program('ddfc_cxx', var='DDFC_CXX', mandatory = True)
    conf.find_program('ddfc_java', var='DDFC_JAVA', mandatory = True)

Task.simple_task_type('bproto', 'protoc \
--plugin=protoc-gen-ddf=${DDFC_CXX} \
--ddf_out=${TGT[0].dir(env)} \
-I ${SRC[0].src_dir(env)} ${PROTOC_FLAGS} ${SRC}',
                      color='PINK',
                      before='cc cxx javac',
                      after='proto_b',
                      shell=True)

Task.simple_task_type('bproto_java', 'protoc \
--plugin=protoc-gen-ddf=${DDFC_JAVA} \
--ddf_out=${OUTDIR} \
-I ${SRC[0].src_dir(env)} ${PROTOC_FLAGS} ${SRC}',
                      color='PINK',
                      before='cc cxx javac',
                      after='proto_b',
                      shell=True)

def bproto_file(self, node):

    if self.proto_gen_java:
        protoc = self.create_task('bproto_java')
        protoc.env['PROTOC_FLAGS'] = get_protoc_flags(self)

        protoc.set_inputs(node)

        proto_text = open(node.abspath(), 'rb').read()
        package_name = re.findall('\W*option\W+java_package\W*=\W*"(.*)"', proto_text)
        ddf_package_name = re.findall('\W*option\W+ddf_java_package\W*=\W*"(.*)"', proto_text)
        java_outer_classname = re.findall('\W*option\W+java_outer_classname\W*=\W*"(.*)"', proto_text)

        if not package_name:
            raise Utils.WafError("No 'package_name' found in: %s" % node.abspath())

        if not ddf_package_name:
            raise Utils.WafError("No 'ddf_package_name' found in: %s" % node.abspath())

        if not java_outer_classname:
            raise Utils.WafError("No 'java_outer_classname' found in: %s" % node.abspath())

        package_name = package_name[0]
        ddf_package_name = ddf_package_name[0]
        java_outer_classname = java_outer_classname[0]

        java_class_file = 'generated/%s/%s.java' % (ddf_package_name.replace('.', '/'), java_outer_classname)
        # find_or_declare(.) is not sufficient here as the
        # package directory in build path may not be present in source dir
        java_node = node.parent.exclusive_build_node(java_class_file)

        self.ddf_javaclass_dirs.add(node.parent.find_dir('generated').abspath(self.env))
        self.ddf_javaclass_inputs.append(java_node.change_ext('.class'))

        outdir = os.path.dirname(node.change_ext('.cpp').abspath(self.env))
        # Set variables for javac
        protoc.env['OUTDIR'] = '%s/generated' % outdir

        if self.env['CLASSPATH'] == []:
            self.env['CLASSPATH'] = ''
        self.env['CLASSPATH'] = self.env['CLASSPATH'] + os.pathsep + "." + os.pathsep + self.classpath

        protoc.set_outputs([java_node])

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

    if self.proto_gen_java:
        java_node_out = java_node.change_ext('.class')
        javac = self.create_task('javac')
        javac.set_inputs(java_node)
        javac.set_outputs(java_node_out)


Task.simple_task_type('proto_b', 'protoc -o${TGT} -I ${SRC[0].src_dir(env)} ${PROTOC_FLAGS} ${SRC}',
                      color='PINK',
                      before='cc cxx',
                      shell=True)


Task.simple_task_type('proto_ddf', 'protoc \
--plugin=protoc-gen-ddf=${DDFC} \
--ddf_out=${TGT[0].dir(env)} \
--python_out=${TGT[0].dir(env)} \
--cpp_out=${TGT[0].dir(env)} \
--java_out=${TGT[0].dir(env)}/generated \
-I ${SRC[0].src_dir(env)} ${PROTOC_FLAGS} ${SRC}',
                      color='RED',
                      before='cc cxx',
                      after='proto_b',
                      shell=True)

Task.simple_task_type('proto_gen_cc', 'protoc --cpp_out=${TGT[0].dir(env)} -I ${SRC[0].src_dir(env)} ${PROTOC_FLAGS} ${SRC}',
                      color='RED',
                      before='cc cxx',
                      after='proto_b',
                      shell=True)

Task.simple_task_type('proto_gen_py', 'protoc --python_out=${TGT[0].dir(env)} -I ${SRC[0].src_dir(env)} ${PROTOC_FLAGS} ${SRC}',
                      color='RED',
                      before='cc cxx',
                      after='proto_b',
                      shell=True)

Task.simple_task_type('proto_gen_java', 'protoc --java_out=${JAVA_OUT} -I ${SRC[0].src_dir(env)} ${PROTOC_FLAGS} ${SRC}',
                      color='RED',
                      before='cc cxx',
                      after='proto_b',
                      shell=True)

def compile_java_file(self, java_node, outdir):
    # Set variables for javac
    # TODO: self.env problem here (global)?
    self.env['OUTDIR'] = outdir
    java_node_out = java_node.change_ext('.class')
    javac = self.create_task('javac')
    javac.set_inputs(java_node)
    javac.set_outputs(java_node_out)

def get_protoc_flags(self):
    protoc_includes = [self.env['DYNAMO_HOME'] + '/ext/include', self.env['DYNAMO_HOME'] + '/share/proto']
    if hasattr(self, "protoc_includes"):
        protoc_includes.extend(Utils.to_list(self.protoc_includes))

    protoc_flags = ""
    for pi in protoc_includes:
        if os.path.isabs(pi):
            p = pi
        else:
            p = self.path.find_dir(pi).srcpath(self.env)

        protoc_flags += " -I %s " % p
    return protoc_flags

@extension('.proto__')
def proto_file__(self, node):
    task = self.create_task('proto_ddf')
    task.set_inputs(node)

    out = [ node.change_ext(x) for x in '.cpp .pb.cc .pb.h _pb2.py'.split() ]
    task.set_outputs(out)

    task.env['PROTOC_FLAGS'] = get_protoc_flags(self)

    self.allnodes.append(node.change_ext('.cpp'))

    if self.proto_compile_cc:
        self.allnodes.append(node.change_ext('.pb.cc'))

@extension('.proto')
def proto_file(self, node):
        Utils.def_attrs(self, java_package=None, classpath='.')
        bproto_file(self, node)

        task = self.create_task('proto_b')
        task.set_inputs(node)
        out = node.change_ext('.bproto')

        protoc_includes = [self.env['DYNAMO_HOME'] + '/ext/include', self.env['DYNAMO_HOME'] + '/share/proto']
        if hasattr(self, "protoc_includes"):
            protoc_includes.extend(Utils.to_list(self.protoc_includes))

        protoc_flags = ""
        for pi in protoc_includes:
            if os.path.isabs(pi):
                p = pi
            else:
                p = self.path.find_dir(pi).srcpath(self.env)

            protoc_flags += " -I %s " % p

        task.env['PROTOC_FLAGS'] = protoc_flags

        if self.install_path:
            self.bld.install_files('${PREFIX}/share/proto', out.abspath(self.env), self.env)

        if self.install_path:
            self.bld.install_files('${PREFIX}/include/' + Utils.g_module.APPNAME,
                                   out.abspath(self.env).replace(".bproto", ".h"), self.env)

        task.set_outputs(out)

        # WHY?!
        if not hasattr(self, "compiled_tasks"):
            self.compiled_tasks = []

        if hasattr(self, "proto_gen_cc") and self.proto_gen_cc:
            task = self.create_task('proto_gen_cc')
            task.env['PROTOC_FLAGS'] = protoc_flags
            task.set_inputs(node)
            cc_out = node.change_ext('.pb.cc')
            h_out = node.change_ext('.pb.h')
            task.set_outputs([cc_out, h_out])
            if hasattr(self, "proto_compile_cc") and self.proto_compile_cc:
                self.allnodes.append(cc_out)

        if hasattr(self, "proto_gen_py") and self.proto_gen_py:
            task = self.create_task('proto_gen_py')
            task.env['PROTOC_FLAGS'] = protoc_flags
            task.set_inputs(node)
            py_out = node.change_ext('_pb2.py')
            if self.install_path:
                self.bld.install_files('${PREFIX}/lib/python', py_out.abspath(self.env), self.env)
            task.set_outputs(py_out)

        if hasattr(self, "proto_gen_java") and self.proto_gen_java:
            task = self.create_task('proto_gen_java')
            task.env['PROTOC_FLAGS'] = protoc_flags
            task.set_inputs(node)

            proto_text = open(node.abspath(), 'rb').read()
            package_name = re.findall('\W*option\W+java_package\W*=\W*"(.*)"', proto_text)
            ddf_package_name = re.findall('\W*option\W+ddf_java_package\W*=\W*"(.*)"', proto_text)
            java_outer_classname = re.findall('\W*option\W+java_outer_classname\W*=\W*"(.*)"', proto_text)

            if not package_name:
                raise Utils.WafError("No 'package_name' found in: %s" % node.abspath())

            if not ddf_package_name:
                raise Utils.WafError("No 'ddf_package_name' found in: %s" % node.abspath())

            if not java_outer_classname:
                raise Utils.WafError("No 'java_outer_classname' found in: %s" % node.abspath())

            package_name = package_name[0]
            ddf_package_name = ddf_package_name[0]
            java_outer_classname = java_outer_classname[0]

            package_dir = package_name.replace('.', '/')
            out.java_package = ddf_package_name
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

