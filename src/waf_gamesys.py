import Task, TaskGen, Utils, re, os
from TaskGen import extension
import hashlib
import waf_ddf

proto_module_sigs = {}
def proto_compile_task(name, module, msg_type, input_ext, output_ext, transformer = None, append_to_all = False):

    def compile(task):
        import google.protobuf.text_format
        mod = __import__(module)
        # NOTE: We can't use getattr. msg_type could of form "foo.bar"
        msg = eval('mod.' + msg_type)() # Call constructor on message type
        with open(task.inputs[0].srcpath(task.env), 'rb') as in_f:
            google.protobuf.text_format.Merge(in_f.read(), msg)

        if transformer:
            msg = transformer(msg)

        with open(task.outputs[0].bldpath(task.env), 'wb') as out_f:
            out_f.write(msg.SerializeToString())

        return 0

    task = Task.task_type_from_func(name,
                                    func    = compile,
                                    color   = 'RED',
                                    after='proto_gen_py',
                                    before='cc cxx')

    m = hashlib.md5()
    m.update(name)
    m.update(module)
    m.update(msg_type)
    if transformer:
        m.update(transformer.func_code.co_code)
        m.update(str(transformer.func_code.co_consts))
    sig_deps = m.digest()

    old_sig_explicit_deps = task.sig_explicit_deps
    def sig_explicit_deps(self):
        if not module in proto_module_sigs:
            mod_m = hashlib.md5()
            # NOTE
            # This has some performance impact
            # Should only be performed when the source file has changed
            # Something similar to scan
            mod = __import__(module)
            mod_file = mod.__file__
            if mod_file.endswith('.pyc'):
                mod_file = mod_file.replace('.pyc', '.py')

            with open(mod_file, 'rb') as f:
                mod_m.update(f.read())
            proto_module_sigs[module] = mod_m.digest()

        deps = old_sig_explicit_deps(self)
        m = hashlib.md5()
        m.update(proto_module_sigs[module])
        m.update(deps)
        m.update(sig_deps)
        return m.digest()

    task.sig_explicit_deps = sig_explicit_deps

    @extension(input_ext)
    def xfile(self, node):
        obj_ext = output_ext
        t = self.create_task(name)
        t.set_inputs(node)
        out = node.change_ext(obj_ext)
        if append_to_all:
            self.allnodes.append(out)
        t.set_outputs(out)

def ProtoCTask(name, message_type, proto_file, input_ext, output_ext, append_to_all = False, include = '../proto'):
    task = Task.simple_task_type(name, 'protoc --encode=%s -I ${DYNAMO_HOME}/share/proto -I %s -I ${DYNAMO_HOME}/ext/include %s < ${SRC} > ${TGT}' % (message_type, include, proto_file),
                                 color='PINK',
                                 after='proto_gen_py',
                                 before='cc cxx',
                                 shell=True)

    def scan(self):
        n = self.generator.path.find_resource(Utils.subst_vars(proto_file, self.env))
        if n:
            deps = waf_ddf.do_scan(self, [n], include)[0]
            deps += [n]
            return deps, []
        else:
            return [], []

    task.scan = scan

    @extension(input_ext)
    def xfile(self, node):
        obj_ext = output_ext
        t = self.create_task(name)
        t.set_inputs(node)
        out = node.change_ext(obj_ext)
        if append_to_all:
            self.allnodes.append(out)
        t.set_outputs(out)

def transform_gameobject(msg):
    for c in msg.Components:
        if (c.Resource.endswith('.camera')):
            c.Resource = c.Resource.replace('.camera', '.camerac')
        c.Resource = c.Resource.replace('.model', '.modelc')
        c.Resource = c.Resource.replace('.script', '.scriptc')
    return msg

def transform_model(msg):
    msg.Material = msg.Material.replace('.material', '.materialc')
    msg.Texture0 = msg.Texture0.replace('.png', '.texture')
    msg.Texture0 = msg.Texture0.replace('.tga', '.texture')
    return msg

def transform_emitter(msg):
    msg.Material = msg.Material.replace('.material', '.materialc')
    msg.Texture.Name = msg.Texture.Name.replace('.png', '.texture')
    msg.Texture.Name = msg.Texture.Name.replace('.tga', '.texture')
    return msg

Task.simple_task_type('gameobjectdesc', 'protoc --encode=dmGameObject.PrototypeDesc -I ${DYNAMO_HOME}/share/proto -I ${DYNAMO_HOME}/ext/include ${DYNAMO_HOME}/share/proto/gameobject_ddf.proto < ${SRC} > ${TGT}',
                      color='PINK',
                      before='cc cxx',
                      shell=True)

proto_compile_task('collection', 'gameobject_ddf_pb2', 'CollectionDesc', '.collection', '.collectionc')
proto_compile_task('material', 'render.material_ddf_pb2', 'material_ddf_pb2.MaterialDesc', '.material', '.materialc')
proto_compile_task('emitter', 'particle.particle_ddf_pb2', 'particle_ddf_pb2.Emitter', '.emitter', '.emitterc', transform_emitter)
proto_compile_task('model', 'render.model_ddf_pb2', 'model_ddf_pb2.ModelDesc', '.model', '.modelc', transform_model)
proto_compile_task('gameobject',  'gameobject_ddf_pb2', 'PrototypeDesc', '.go', '.goc', transform_gameobject)
proto_compile_task('convexshape',  'physics_ddf_pb2', 'ConvexShape', '.convexshape_pb', '.convexshape')
proto_compile_task('collisionobject',  'physics_ddf_pb2', 'CollisionObjectDesc', '.collisionobject_pb', '.collisionobject')
proto_compile_task('gui',  'gui_ddf_pb2', 'SceneDesc', '.gui', '.guic')

@extension('.go_pb')
def gameobjectdesc_file(self, node):
    obj_ext = '.go'
    task = self.create_task('gameobjectdesc')
    task.set_inputs(node)
    out = node.change_ext(obj_ext)
    task.set_outputs(out)


Task.simple_task_type('luascript', 'cat < ${SRC} > ${TGT}',
                      color='PINK',
                      before='cc cxx',
                      shell=True)

@extension('.script')
def testresourcecont_file(self, node):
    obj_ext = '.scriptc'
    task = self.create_task('luascript')
    task.set_inputs(node)
    out = node.change_ext(obj_ext)
    task.set_outputs(out)
