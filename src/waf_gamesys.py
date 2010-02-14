import Task, TaskGen
from TaskGen import extension

def ProtoCTask(name, message_type, proto_file, input_ext, output_ext, append_to_all = False, include = '../proto'):
    Task.simple_task_type(name, 'protoc --encode=%s -I ${DYNAMO_HOME}/share/proto -I %s -I ${DYNAMO_HOME}/ext/include %s < ${SRC} > ${TGT}' % (message_type, include, proto_file),
                          color='PINK',
                          after='proto_gen_py',
                          before='cc cxx',
                          shell=True)

    @extension(input_ext)
    def xfile(self, node):
        obj_ext = output_ext
        t = self.create_task(name)
        t.set_inputs(node)
        out = node.change_ext(obj_ext)
        if append_to_all:
            self.allnodes.append(out)
        t.set_outputs(out)

Task.simple_task_type('gameobjectdesc', 'protoc --encode=dmGameObject.PrototypeDesc -I ${DYNAMO_HOME}/share/proto ${DYNAMO_HOME}/share/proto/gameobject_ddf.proto < ${SRC} > ${TGT}',
                      color='PINK',
                      before='cc cxx',
                      shell=True)

@extension('.go_pb')
def gameobjectdesc_file(self, node):
    obj_ext = '.go'
    task = self.create_task('gameobjectdesc')
    task.set_inputs(node)
    out = node.change_ext(obj_ext)
    task.set_outputs(out)


Task.simple_task_type('pyscript', 'cat < ${SRC} > ${TGT}',
                      color='PINK',
                      before='cc cxx',
                      shell=True)

@extension('.script')
def testresourcecont_file(self, node):
    obj_ext = '.scriptc'
    task = self.create_task('pyscript')
    task.set_inputs(node)
    out = node.change_ext(obj_ext)
    task.set_outputs(out)

Task.simple_task_type('convexshape', 'protoc --encode=dmPhysics.ConvexShape -I ${DYNAMO_HOME}/share/proto ${DYNAMO_HOME}/share/proto/physics_ddf.proto < ${SRC} > ${TGT}',
                      color='PINK',
                      before='cc cxx',
                      shell=True)

@extension('.convexshape_pb')
def gameobjectdesc_file(self, node):
    obj_ext = '.convexshape'
    task = self.create_task('convexshape')
    task.set_inputs(node)
    out = node.change_ext(obj_ext)
    task.set_outputs(out)

Task.simple_task_type('rigidbody', 'protoc --encode=dmPhysics.RigidBodyDesc -I ${DYNAMO_HOME}/share/proto ${DYNAMO_HOME}/share/proto/physics_ddf.proto < ${SRC} > ${TGT}',
                      color='PINK',
                      before='cc cxx',
                      shell=True)

@extension('.rigidbody_pb')
def gameobjectdesc_file(self, node):
    obj_ext = '.rigidbody'
    task = self.create_task('rigidbody')
    task.set_inputs(node)
    out = node.change_ext(obj_ext)
    task.set_outputs(out)

