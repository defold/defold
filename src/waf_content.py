import Task
import Utils
from TaskGen import extension, before, feature
import hashlib, sys, os

# NOTE: This must be included explicitly prior to any modules with fields that set [(resource) = true].
# Otherwise field_desc.GetOptions().ListFields() will return [] for the first loaded module
# Strange error and probably a bug in google protocol buffers
import ddf.ddf_extensions_pb2

@feature('*')
@before('apply_core')
def apply_content_root(self):
    Utils.def_attrs(self, content_root = '.')
    if not self.path.find_dir(self.content_root):
        raise Utils.WafError('content_root "%s" not found (relative to %s)', self.content_root, self.path)
    self.content_root = self.path.find_dir(self.content_root).srcpath(self.env)

proto_module_sigs = {}
def proto_compile_task(name, module, msg_type, input_ext, output_ext, transformer = None, append_to_all = False):

    # NOTE: This could be cached
    def is_resource(field_desc):
        for options_field_desc, value in field_desc.GetOptions().ListFields():
            if options_field_desc.name == 'resource' and value:
                return True
        return False

    def validate_resource_files(task, msg):
        import google.protobuf
        from google.protobuf.descriptor import FieldDescriptor

        descriptor = getattr(msg, 'DESCRIPTOR')
        for field in descriptor.fields:
            value = getattr(msg, field.name)
            if field.type == FieldDescriptor.TYPE_MESSAGE:
                if field.label == FieldDescriptor.LABEL_REPEATED:
                    for x in value:
                        validate_resource_files(task, x)
                else:
                    validate_resource_files(task, value)
            elif is_resource(field):

                if field.label == FieldDescriptor.LABEL_REPEATED:
                    lst = value
                else:
                    lst = [value]

                for x in lst:
                    if field.label == FieldDescriptor.LABEL_OPTIONAL and len(x) == 0:
                        # Skip not specified optional fields
                        # These are accepted as "resources"
                        # NOTE: Somewhat strange to test this predicate in a loop
                        # as optional can't be repeated. Anyway it works :-)
                        continue

                    if not x.startswith('/'):
                        print >>sys.stderr, '%s:0: error: resource path is not absolute "%s"' % (task.inputs[0].srcpath(task.env), x)
                        return False
                    path = os.path.join(task.generator.content_root, x[1:])
                    if not os.path.exists(path):
                        print >>sys.stderr, '%s:0: error: is missing dependent resource file "%s"' % (task.inputs[0].srcpath(task.env), x)
                        return False
        return True

    def compile(task):
        try:
            import google.protobuf.text_format
            mod = __import__(module)
            # NOTE: We can't use getattr. msg_type could of form "foo.bar"
            msg = eval('mod.' + msg_type)() # Call constructor on message type
            with open(task.inputs[0].srcpath(task.env), 'rb') as in_f:
                google.protobuf.text_format.Merge(in_f.read(), msg)

            if not validate_resource_files(task, msg):
                return 1

            if transformer:
                msg = transformer(task, msg)

            with open(task.outputs[0].bldpath(task.env), 'wb') as out_f:
                out_f.write(msg.SerializeToString())

            return 0
        except google.protobuf.text_format.ParseError,e:
            print >>sys.stderr, '%s:%s' % (task.inputs[0].srcpath(task.env), str(e))
            return 1

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

    def scan_msg(task, msg):
        import google.protobuf
        from google.protobuf.descriptor import FieldDescriptor
        depnodes = []

        descriptor = getattr(msg, 'DESCRIPTOR')
        for field in descriptor.fields:
            value = getattr(msg, field.name)
            if field.type == FieldDescriptor.TYPE_MESSAGE:
                if field.label == FieldDescriptor.LABEL_REPEATED:
                    for x in value:
                        scan_msg(task, x)
                else:
                    scan_msg(task, value)
            elif is_resource(field):

                if field.label == FieldDescriptor.LABEL_REPEATED:
                    lst = value
                else:
                    lst = [value]

                for x in lst:
                    # NOTE: find_resource doesn't handle unicode string. Thats why str(.) is required
                    n = task.generator.path.find_resource(str(x[1:]))
                    if n:
                        depnodes.append(n)
        return depnodes

    def scan(task):
        # NOTE: The scanner treats all resource as dependences.
        # The only actual required dependencies are the embedded ones but
        # better be safe than sorry! :-)
        depnodes = []
        for n in task.inputs:
            depnodes.append(n)

            try:
                import google.protobuf.text_format
                mod = __import__(module)
                # NOTE: We can't use getattr. msg_type could of form "foo.bar"
                msg = eval('mod.' + msg_type)() # Call constructor on message type
                with open(n.srcpath(task.env), 'rb') as in_f:
                    google.protobuf.text_format.Merge(in_f.read(), msg)

                depnodes += scan_msg(task, msg)

            except google.protobuf.text_format.ParseError,e:
                # We ignore parse error as the file will hopefully be changed and recompiled anyway
                pass

        return depnodes, []

    task.sig_explicit_deps = sig_explicit_deps
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
