# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

import waflib.Task
import waflib.Utils
import waflib.Errors
from waflib.TaskGen import extension, before, feature
import hashlib, sys, os, importlib

# NOTE: This must be included explicitly prior to any modules with fields that set [(resource) = true].
# Otherwise field_desc.GetOptions().ListFields() will return [] for the first loaded module
# Strange error and probably a bug in google protocol buffers
import ddf.ddf_extensions_pb2

from importlib.machinery import PathFinder

class DefoldImporter(PathFinder):
    def find_spec(self, fullname, path=None, target=None):
        if not "ddf_pb2" in fullname:
            return None

        tokens = fullname.split(".")
        subpath = os.path.sep.join(tokens) + ".py"
        spec = None
        for x in sys.path:
            p = os.path.join(x, subpath)
            if not os.path.exists(p):
                continue
            spec = importlib.util.spec_from_file_location(fullname, p)
            if spec is not None:
                break
        return spec

sys.meta_path.insert(0, DefoldImporter())


@feature('*')
@before('apply_core')
def apply_content_root(self):
    waflib.Utils.def_attrs(self, content_root = '.')
    self.content_root = self.path.find_dir(self.content_root).abspath()

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
        from collections.abc import Mapping

        def _validate_field(task, field, value):
            # Handle regular message fields (non-map)
            if field.type == FieldDescriptor.TYPE_MESSAGE and not (field.message_type and field.message_type.GetOptions().map_entry):
                if field.label == FieldDescriptor.LABEL_REPEATED:
                    for x in value:
                        if isinstance(x, google.protobuf.message.Message):
                            if not validate_resource_files(task, x):
                                return False
                else:
                    if isinstance(value, google.protobuf.message.Message):
                        if not validate_resource_files(task, value):
                            return False
                return True

            # Handle map<key, value> fields
            if field.type == FieldDescriptor.TYPE_MESSAGE and field.message_type and field.message_type.GetOptions().map_entry:
                # Map fields are represented as a Mapping (MapContainer)
                if isinstance(value, Mapping):
                    entry_desc = field.message_type
                    key_field = entry_desc.fields_by_name.get('key')
                    value_field = entry_desc.fields_by_name.get('value')
                    for k, v in value.items():
                        if key_field:
                            if not _validate_field(task, key_field, k):
                                return False
                        if value_field:
                            if not _validate_field(task, value_field, v):
                                return False
                return True

            # Handle scalar resource fields
            if is_resource(field):

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
                        print ('%s:0: error: resource path is not absolute "%s"' % (task.inputs[0].srcpath(), x), file=sys.stderr)
                        return False
                    path = os.path.join(task.generator.content_root, x[1:])
                    if not os.path.exists(path):
                        print ('%s:0: error: is missing dependent resource file "%s"' % (task.inputs[0].srcpath(), x), file=sys.stderr)
                        return False
            return True

        descriptor = getattr(msg, 'DESCRIPTOR')
        for field in descriptor.fields:
            value = getattr(msg, field.name)
            if not _validate_field(task, field, value):
                return False
        return True

    def compile(task):
        try:
            import google.protobuf.text_format
            mod = __import__(module)
            # NOTE: We can't use getattr. msg_type could of form "foo.bar"
            msg = eval('mod.' + msg_type)() # Call constructor on message type
            with open(task.inputs[0].srcpath(), 'rb') as in_f:
                google.protobuf.text_format.Merge(in_f.read(), msg)

            if not validate_resource_files(task, msg):
                return 1

            if transformer:
                msg = transformer(task, msg)

            with open(task.outputs[0].abspath(), 'wb') as out_f:
                out_f.write(msg.SerializeToString())

            return 0
        except (google.protobuf.text_format.ParseError,e):
            print ('%s:%s' % (task.inputs[0].srcpath(), str(e)), file=sys.stderr)
            return 1

        except (google.protobuf.message.EncodeError,e):
            print ('%s:%s' % (task.inputs[0].srcpath(), str(e)), file=sys.stderr)
            return 1


    task = waflib.Task.task_factory(name,
                                    func    = compile,
                                    color   = 'RED',
                                    after='proto_gen_py',
                                    before='c cxx')

    m = hashlib.md5()
    m.update(name.encode('ascii'))
    m.update(module.encode('ascii'))
    m.update(msg_type.encode('ascii'))
    if transformer:
        m.update(transformer.__code__.co_code)
        m.update(str(transformer.__code__.co_consts).encode('ascii'))
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
        if deps != None:
            m.update(deps)
        m.update(sig_deps)
        return m.digest()

    def scan_msg(task, msg):
        import google.protobuf
        from google.protobuf.descriptor import FieldDescriptor
        from collections.abc import Mapping
        depnodes = []

        def _scan_field(task, field, value):
            local_depnodes = []

            # Handle regular message fields (non-map)
            if field.type == FieldDescriptor.TYPE_MESSAGE and not (field.message_type and field.message_type.GetOptions().map_entry):
                if field.label == FieldDescriptor.LABEL_REPEATED:
                    for x in value:
                        if isinstance(x, google.protobuf.message.Message):
                            local_depnodes.extend(scan_msg(task, x))
                else:
                    if isinstance(value, google.protobuf.message.Message):
                        local_depnodes.extend(scan_msg(task, value))
                return local_depnodes

            # Handle map<key, value> fields
            if field.type == FieldDescriptor.TYPE_MESSAGE and field.message_type and field.message_type.GetOptions().map_entry:
                if isinstance(value, Mapping):
                    entry_desc = field.message_type
                    key_field = entry_desc.fields_by_name.get('key')
                    value_field = entry_desc.fields_by_name.get('value')
                    for k, v in value.items():
                        if key_field:
                            local_depnodes.extend(_scan_field(task, key_field, k))
                        if value_field:
                            local_depnodes.extend(_scan_field(task, value_field, v))
                return local_depnodes

            # Handle scalar resource fields
            if is_resource(field):

                if field.label == FieldDescriptor.LABEL_REPEATED:
                    lst = value
                else:
                    lst = [value]

                for x in lst:
                    # NOTE: find_resource doesn't handle unicode string. Thats why str(.) is required
                    n = task.generator.path.find_resource(str(x[1:]))
                    if n:
                        local_depnodes.append(n)
            return local_depnodes

        descriptor = getattr(msg, 'DESCRIPTOR')
        for field in descriptor.fields:
            value = getattr(msg, field.name)
            depnodes.extend(_scan_field(task, field, value))
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
                with open(n.srcpath(), 'rb') as in_f:
                    google.protobuf.text_format.Merge(in_f.read(), msg)

                depnodes += scan_msg(task, msg)

            except google.protobuf.text_format.ParseError as e:
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
