import Task
from TaskGen import extension
import hashlib

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
