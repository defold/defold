import Task, TaskGen
from TaskGen import extension

Task.simple_task_type('bproto', 'python ${DDF_PY} ${ddf_options} ${SRC} -o ${TGT}',
                      color='PINK', 
                      before='cc cxx',
                      shell=True)

@extension('.bproto')
def bproto_file(self, node):
        obj_ext = '.cpp'

        protoc = self.create_task('bproto')
        protoc.set_inputs(node)
        out = node.change_ext(obj_ext)
        protoc.set_outputs(out)
        self.allnodes.append(out) # Always???
        if hasattr(self, "ddf_namespace"):
            self.env['ddf_options'] = '--ns %s' % self.ddf_namespace

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

@extension('.proto')
def proto_file(self, node):
        task = self.create_task('proto_b')
        task.set_inputs(node)
        out = node.change_ext('.bproto')
        self.bld.install_files('${PREFIX}/share/proto', out.abspath(self.env), self.env)
        task.set_outputs(out)
        self.allnodes.append(out)
        
        # WHY?!
        if not hasattr(self, "compiled_tasks"):
            self.compiled_tasks = []
        
        if hasattr(self, "proto_gen_cc") and self.proto_gen_cc:
            task = self.create_task('proto_gen_cc')
            task.set_inputs(node)
            out = node.change_ext('.pb.cc')
            task.set_outputs(out)
            if hasattr(self, "proto_compile_cc") and self.proto_compile_cc:
                self.allnodes.append(out)

        if hasattr(self, "proto_gen_py") and self.proto_gen_py:
            task = self.create_task('proto_gen_py')
            task.set_inputs(node)
            out = node.change_ext('_pb2.py')
            self.bld.install_files('${PREFIX}/lib/python', out.abspath(self.env), self.env)
            task.set_outputs(out)
