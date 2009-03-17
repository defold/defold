#! /usr/bin/env python

VERSION='0.1'
APPNAME='ddf'

srcdir = '.'
blddir = 'build'

import os, sys

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
                      shell=True)

Task.simple_task_type('proto_gen_py', 'protoc --python_out=${TGT[0].dir(env)} -I ${SRC[0].src_dir(env)} ${SRC}', 
                      color='RED', 
                      before='cc cxx',
                      shell=True)

@extension('.proto')
def proto_file(self, node):
        task = self.create_task('proto_b')
        task.set_inputs(node)
        out = node.change_ext('.bproto')
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
            task.set_outputs(out)

def init():
	pass

def set_options(opt):
	opt.sub_options('src')
	opt.tool_options('compiler_cxx')

def configure(conf):
	conf.check_tool('compiler_cxx')
	conf.sub_config('src')

        # TOOD: Add $DYNAMO_EXT/lib/python to path here?
        ddf_search_lst = ['src']
        if os.getenv('PYTHONPATH'):
                ddf_search_lst += os.getenv('PYTHONPATH').split(':')
        for d in ddf_search_lst:
                if os.path.isfile(os.path.join(d, "ddf.py")):
                        conf.env['DDF_PY'] = os.path.abspath(os.path.join(d, "ddf.py"))

        if not conf.env['DDF_PY']:
                conf.fatal("ddf.py not found in: " + str(ddf_search_lst))
                
        if sys.platform == "darwin":
            platform = "darwin"
        elif sys.platform == "linux2":
            platform = "linux"
        elif sys.platform == "win32":
            platform = "win32"
        else:
            conf.fatal("Unable to determine platform")

        if platform == "linux" or platform == "darwin":
            conf.env['CXXFLAGS']='-g -D__STDC_LIMIT_MACROS'
        else:
            conf.env['CXXFLAGS']=['/Z7', '/MT', '/D__STDC_LIMIT_MACROS']
            conf.env.append_value('CPPPATH', "../src/win32") 


        dynamo_ext = os.getenv('DYNAMO_EXT')
        if not dynamo_ext:
                conf.fatal("DYNAMO_EXT not set")

        # TODO: WTF!!
        conf.env.append_value('CPPPATH', "../src") 
        conf.env.append_value('CPPPATH', os.path.join(dynamo_ext, "include"))
        conf.env.append_value('CPPPATH', ".")
        conf.env.append_value('LIBPATH', os.path.join(dynamo_ext, "lib", platform))

        conf.env['LIB_PROTOBUF'] = 'protobuf'
        conf.env['LIB_GTEST'] = 'gtest'

def build(bld):
	bld.add_subdirs('src')

