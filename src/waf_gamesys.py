import Task, TaskGen
from TaskGen import extension

Task.simple_task_type('gameobjectdesc', 'protoc --encode=GameObject.GameObjectPrototypeDesc -I ${DYNAMO_HOME}/share/proto ${DYNAMO_HOME}/share/proto/gameobject_ddf.proto < ${SRC} > ${TGT}',
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

