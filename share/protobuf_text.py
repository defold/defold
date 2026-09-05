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

from functools import lru_cache

from google.protobuf import descriptor_pb2, descriptor_pool, message_factory, text_format


def _is_gameobject_source_payload_field(field) -> bool:
    return (
        field is not None
        and field.containing_oneof is not None
        and field.containing_oneof.name == "payload"
        and field.containing_type.file.package == "dmGameObjectSourceDDF"
    )


class _GameObjectSourceParser(text_format._Parser):
    """Merge-compatible parser that rejects ambiguous source payloads."""

    def _MergeField(self, tokenizer, message):
        field = message.DESCRIPTOR.fields_by_name.get(tokenizer.token)
        if _is_gameobject_source_payload_field(field):
            selected_field = message.WhichOneof(field.containing_oneof.name)
            if selected_field is not None and selected_field != field.name:
                raise tokenizer.ParseError(
                    'Field "%s" is specified along with field "%s", another '
                    'member of oneof "%s" for message type "%s".'
                    % (
                        field.name,
                        selected_field,
                        field.containing_oneof.name,
                        message.DESCRIPTOR.full_name,
                    )
                )
        return super()._MergeField(tokenizer, message)


@lru_cache(maxsize=None)
def _source_read_class(source_descriptor):
    # Released Defold versions embed protobuf text in a string data field.
    pool = descriptor_pool.DescriptorPool()

    def add_file(file):
        for dependency in file.dependencies:
            add_file(dependency)
        proto = descriptor_pb2.FileDescriptorProto.FromString(file.serialized_pb)
        if proto.package == "dmGameObjectSourceDDF":
            for message in proto.message_type:
                if message.name in ("EmbeddedComponentDesc", "EmbeddedInstanceDesc"):
                    message.field.add(name="data", number=len(message.field) + 1, oneof_index=0,
                                      label=1, type=9)
        pool.Add(proto)

    add_file(source_descriptor.file)
    return message_factory.GetMessageClass(pool.FindMessageTypeByName(source_descriptor.full_name))


def _copy_source(parsed, target):
    for field, value in parsed.ListFields():
        target_field = target.DESCRIPTOR.fields_by_name.get(field.name)
        if target_field is None:
            if field.name == "data":
                if target.DESCRIPTOR.name == "EmbeddedInstanceDesc":
                    merge_gameobject_source_text(value, target.prototype)
                    target.prototype.SetInParent()
                else:
                    target.component_data.data.string = value
        elif field.message_type is None:
            if field.is_repeated:
                getattr(target, field.name).extend(value)
            else:
                setattr(target, field.name, value)
        elif field.message_type.file.package != "dmGameObjectSourceDDF":
            if field.is_repeated:
                for child in value:
                    getattr(target, field.name).add().ParseFromString(child.SerializeToString())
            else:
                getattr(target, field.name).ParseFromString(value.SerializeToString())
        elif field.is_repeated:
            for child in value:
                _copy_source(child, getattr(target, field.name).add())
        else:
            child = getattr(target, field.name)
            _copy_source(value, child)
            child.SetInParent()


def merge_gameobject_source_text(text, message):
    """Read legacy and structured source, rejecting conflicting payload arms."""
    source_format = message.DESCRIPTOR.file.package == "dmGameObjectSourceDDF"
    parsed = _source_read_class(message.DESCRIPTOR)() if source_format else message
    parser = _GameObjectSourceParser()
    lines = text.split(b"\n" if isinstance(text, bytes) else "\n")
    parser.MergeLines(lines, parsed)
    if source_format:
        _copy_source(parsed, message)
    return message


def component_data_message(data, message):
    """Decode shared component data using the component's own protobuf schema."""
    if data.data.WhichOneof("kind") == "string":
        return merge_gameobject_source_text(data.data.string, message)
    if message.DESCRIPTOR.full_name == "dmGameSystemDDF.Data":
        message.CopyFrom(data)
        return message

    import base64

    def scalar(value, field):
        if field.type == field.TYPE_BYTES:
            return base64.b64decode(value.string)
        if field.type == field.TYPE_ENUM:
            return field.enum_type.values_by_name[value.string].number
        if field.type in (field.TYPE_INT64, field.TYPE_UINT64, field.TYPE_SINT64, field.TYPE_FIXED64, field.TYPE_SFIXED64):
            return int(value.string)
        if field.type in (field.TYPE_INT32, field.TYPE_UINT32, field.TYPE_SINT32, field.TYPE_FIXED32, field.TYPE_SFIXED32):
            return int(value.number)
        return getattr(value, value.WhichOneof("kind"))

    def merge_struct(value, target):
        if value.WhichOneof("kind") != "struct":
            raise ValueError("Expected structured component data")
        for name, item in value.struct.fields.items():
            field = target.DESCRIPTOR.fields_by_name[name]
            if field.containing_oneof and target.WhichOneof(field.containing_oneof.name):
                raise ValueError("Conflicting component data fields in oneof " + field.containing_oneof.name)
            if field.is_repeated:
                if item.WhichOneof("kind") != "list":
                    raise ValueError("Expected list for " + name)
                container = getattr(target, name)
                for element in item.list.values:
                    if field.message_type and field.message_type.GetOptions().map_entry:
                        entry = message_factory.GetMessageClass(field.message_type)()
                        merge_struct(element, entry)
                        if field.message_type.fields_by_name["value"].message_type:
                            container[entry.key].CopyFrom(entry.value)
                        else:
                            container[entry.key] = entry.value
                    elif field.message_type:
                        merge_struct(element, container.add())
                    else:
                        container.append(scalar(element, field))
            elif field.message_type:
                child = getattr(target, name)
                merge_struct(item, child)
                child.SetInParent()
            else:
                setattr(target, name, scalar(item, field))

    merge_struct(data.data, message)
    return message


def message_to_component_data(message, data):
    """Encode extension fields without embedding a protobuf text document."""
    if message.DESCRIPTOR.full_name == "dmGameSystemDDF.Data":
        data.CopyFrom(message)
        return

    import base64

    def encode_value(value, field, target):
        if field.message_type:
            encode_message(value, target)
        elif field.type == field.TYPE_BYTES:
            target.string = base64.b64encode(value).decode("ascii")
        elif field.type == field.TYPE_ENUM:
            target.string = field.enum_type.values_by_number[value].name
        elif field.type in (field.TYPE_INT64, field.TYPE_UINT64, field.TYPE_SINT64, field.TYPE_FIXED64, field.TYPE_SFIXED64):
            target.string = str(value)
        elif field.type == field.TYPE_BOOL:
            target.bool = value
        elif field.type == field.TYPE_STRING:
            target.string = value
        else:
            target.number = value

    def encode_message(source, target):
        target.struct.SetInParent()
        for field, value in source.ListFields():
            item = target.struct.fields[field.name]
            if field.is_repeated:
                item.list.SetInParent()
                if field.message_type and field.message_type.GetOptions().map_entry:
                    for key, map_value in value.items():
                        entry = message_factory.GetMessageClass(field.message_type)()
                        entry.key = key
                        if field.message_type.fields_by_name["value"].message_type:
                            entry.value.CopyFrom(map_value)
                        else:
                            entry.value = map_value
                        encode_message(entry, item.list.values.add())
                else:
                    for element in value:
                        encode_value(element, field, item.list.values.add())
            else:
                encode_value(value, field, item)

    data.Clear()
    encode_message(message, data.data)
