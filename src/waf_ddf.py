import os, re
import Task, TaskGen
from TaskGen import extension
import Utils

def configure(conf):
    conf.find_file('ddfc.py', var='DDFC', mandatory = True)

Task.simple_task_type('bproto_java', 'python ${DDFC} ${ddf_options} ${SRC} \
 --java_package=${JAVA_PACKAGE} --java_out=${TGT[0].dir(env)}/generated --java_classname=${JAVA_CLASSNAME} \
 -o ${TGT}',
                      color='PINK',
                      before='cc cxx javac',
                      after='proto_b',
                      shell=True)

Task.simple_task_type('bproto', 'python ${DDFC} ${ddf_options} ${SRC} -o ${TGT}',
                      color='PINK',
                      before='cc cxx javac',
                      after='proto_b',
                      shell=True)

@extension('.bproto')
def bproto_file(self, node):
        Utils.def_attrs(self, java_package=None, classpath='.')

        obj_ext = '.cpp'

        if hasattr(node, 'java_package'):
            protoc = self.create_task('bproto_java')
            protoc.set_inputs(node)
            out = node.change_ext(obj_ext)
            
            protoc.env['JAVA_PACKAGE'] = node.java_package
            protoc.env['JAVA_CLASSNAME'] = node.proto_java_classname

            java_class_file = 'generated/%s/%s.java' % (node.java_package.replace('.', '/'), node.proto_java_classname)
            # find_or_declare(.) is not sufficient here as the
            # package directory in build path may not be present in source dir
            java_node = node.parent.exclusive_build_node(java_class_file)

            outdir = os.path.dirname(out.abspath(self.env))
            # Set variables for javac
            protoc.env['OUTDIR'] = '%s/generated' % outdir

            if self.env['CLASSPATH'] == []:
                self.env['CLASSPATH'] = ''
            self.env['CLASSPATH'] = self.env['CLASSPATH'] + os.pathsep + "." + os.pathsep + self.classpath

            protoc.set_outputs([out, java_node])
        else:
            protoc = self.create_task('bproto')
            protoc.set_inputs(node)
            out = node.change_ext(obj_ext)
            protoc.set_outputs([out])

        self.allnodes.append(out) 
        # TODO: Appending java_node doesn't work. Missing "extension-featre" in built-in tool?
        # An explicit node is create below

        if hasattr(self, "ddf_namespace"):
            protoc.env['ddf_options'] = '--ns %s' % self.ddf_namespace

        if hasattr(node, 'java_package'):
            java_node_out = java_node.change_ext('.class')
            javac = self.create_task('javac')
            javac.set_inputs(java_node)
            javac.set_outputs(java_node_out)

Task.simple_task_type('proto_b', 'protoc -o${TGT} -I ${SRC[0].src_dir(env)} ${SRC}',
                      color='PINK',
                      before='cc cxx',
                      shell=True)

Task.simple_task_type('proto_gen_cc', 'protoc --cpp_out=${TGT[0].dir(env)} -I ${SRC[0].src_dir(env)} ${SRC}',
                      color='RED',
                      before='cc cxx',
                      after='proto_b',
                      shell=True)

Task.simple_task_type('proto_gen_py', 'protoc --python_out=${TGT[0].dir(env)} -I ${SRC[0].src_dir(env)} ${SRC}',
                      color='RED',
                      before='cc cxx',
                      after='proto_b',
                      shell=True)

Task.simple_task_type('proto_gen_java', 'protoc --java_out=${JAVA_OUT} -I ${SRC[0].src_dir(env)} ${SRC}',
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

@extension('.proto')
def proto_file(self, node):
        task = self.create_task('proto_b')
        task.set_inputs(node)
        out = node.change_ext('.bproto')

        if self.install_path:
            self.bld.install_files('${PREFIX}/share/proto', out.abspath(self.env), self.env)

        if self.install_path:
            self.bld.install_files('${PREFIX}/include/' + Utils.g_module.APPNAME,
                                   out.abspath(self.env).replace(".bproto", ".h"), self.env)

        task.set_outputs(out)
        self.allnodes.append(out)

        # WHY?!
        if not hasattr(self, "compiled_tasks"):
            self.compiled_tasks = []

        if hasattr(self, "proto_gen_cc") and self.proto_gen_cc:
            task = self.create_task('proto_gen_cc')
            task.set_inputs(node)
            cc_out = node.change_ext('.pb.cc')
            task.set_outputs(cc_out)
            if hasattr(self, "proto_compile_cc") and self.proto_compile_cc:
                self.allnodes.append(cc_out)

        if hasattr(self, "proto_gen_py") and self.proto_gen_py:
            task = self.create_task('proto_gen_py')
            task.set_inputs(node)
            py_out = node.change_ext('_pb2.py')
            if self.install_path:
                self.bld.install_files('${PREFIX}/lib/python', py_out.abspath(self.env), self.env)
            task.set_outputs(py_out)

        if hasattr(self, "proto_gen_java") and self.proto_gen_java:
            task = self.create_task('proto_gen_java')
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

            compile_java_file(self, java_out, java_out_dir)

