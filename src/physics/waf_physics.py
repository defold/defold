import Task, TaskGen
from TaskGen import extension

Task.simple_task_type('convexshape', 'protoc --encode=Physics.ConvexShape -I ${DYNAMO_HOME}/share/proto ${DYNAMO_HOME}/share/proto/physics_ddf.proto < ${SRC} > ${TGT}',
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

Task.simple_task_type('rigidbody', 'protoc --encode=Physics.RigidBodyDesc -I ${DYNAMO_HOME}/share/proto ${DYNAMO_HOME}/share/proto/physics_ddf.proto < ${SRC} > ${TGT}',
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

def configure(conf):
    conf.env['STATICLIB_PHYSICS'] = ['physics', 'BulletDynamics', 'BulletCollision', 'LinearMath']
