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

from google.protobuf import text_format


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


def merge_gameobject_source_text(text, message):
    """Merge protobuf text while forbidding distinct source payload arms.

    Defold source files historically allow repeated ordinary singular fields,
    so protobuf's strict Parse() cannot be used. Merge() permits those fields,
    but would silently let a later oneof arm replace an earlier arm. This parser
    retains Merge semantics everywhere except GameObjectSource payload oneofs.
    """

    parser = _GameObjectSourceParser()
    lines = text.split(b"\n" if isinstance(text, bytes) else "\n")
    return parser.MergeLines(lines, message)
