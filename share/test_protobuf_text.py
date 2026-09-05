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

import unittest

from google.protobuf import text_format
from ddf import ddf_struct_pb2
from gameobject import gameobject_ddf_pb2
from gameobject_source import gameobject_source_ddf_pb2
from gamesys import data_ddf_pb2, gamesys_ddf_pb2, sprite_ddf_pb2

from protobuf_text import component_data_message, merge_gameobject_source_text, message_to_component_data


class SourceFormatTest(unittest.TestCase):
    def test_released_collection_with_nested_components(self):
        sprite = sprite_ddf_pb2.SpriteDesc(default_animation="Spelare åäö")
        prototype = gameobject_ddf_pb2.PrototypeDesc()
        prototype.embedded_components.add(id="sprite", type="sprite", data=text_format.MessageToString(sprite))
        collection = gameobject_ddf_pb2.CollectionDesc(name="main")
        collection.embedded_instances.add(id="go", data=text_format.MessageToString(prototype))
        source = merge_gameobject_source_text(text_format.MessageToString(collection), gameobject_source_ddf_pb2.CollectionDesc())
        embedded = source.embedded_instances[0]
        self.assertEqual("prototype", embedded.WhichOneof("payload"))
        self.assertEqual(sprite, component_data_message(embedded.prototype.embedded_components[0].component_data,
                                                       sprite_ddf_pb2.SpriteDesc()))

    def test_structured_extension_fields(self):
        for message in (sprite_ddf_pb2.SpriteDesc(default_animation="idle", blend_mode=sprite_ddf_pb2.SpriteDesc.BLEND_MODE_ADD),
                        gamesys_ddf_pb2.FactoryDesc(prototype="/player.go")):
            data = data_ddf_pb2.Data()
            message_to_component_data(message, data)
            self.assertEqual("struct", data.data.WhichOneof("kind"))
            self.assertEqual(message, component_data_message(data, type(message)()))

    def test_map_fields_and_large_integers(self):
        fields = ddf_struct_pb2.Struct()
        fields.fields["empty"].struct.SetInParent()
        fields.fields["values"].list.values.add(bool=False)
        for message in (fields, gameobject_ddf_pb2.ComponenTypeDesc(name_hash=2**64 - 1, max_count=2**32 - 1)):
            data = data_ddf_pb2.Data()
            message_to_component_data(message, data)
            self.assertEqual(message, component_data_message(data, type(message)()))
        self.assertEqual(str(2**64 - 1), data.data.struct.fields["name_hash"].string)

    def test_lights_share_one_payload(self):
        for light_type in ("ambient_light", "directional_light", "point_light", "spot_light"):
            embedded = gameobject_source_ddf_pb2.EmbeddedComponentDesc(id="light", type=light_type)
            embedded.component_data.data.struct.fields["intensity"].number = 2.5
            parsed = merge_gameobject_source_text(text_format.MessageToString(embedded),
                                                 gameobject_source_ddf_pb2.EmbeddedComponentDesc())
            self.assertEqual("component_data", parsed.WhichOneof("payload"))
            self.assertEqual(embedded.component_data, component_data_message(parsed.component_data, data_ddf_pb2.Data()))

    def test_rejects_conflicting_payloads(self):
        for text in ('id: "sprite" type: "sprite" data: "" sprite {}',
                     'id: "sprite" type: "sprite" component_data {} sprite {}'):
            with self.assertRaises(text_format.ParseError):
                merge_gameobject_source_text(text, gameobject_source_ddf_pb2.EmbeddedComponentDesc())

    def test_branch_only_light_payloads_are_not_supported(self):
        with self.assertRaises(text_format.ParseError):
            merge_gameobject_source_text('id: "light" type: "point_light" point_light {}',
                                        gameobject_source_ddf_pb2.EmbeddedComponentDesc())

    def test_empty_released_instance(self):
        instance = merge_gameobject_source_text('id: "go" data: ""', gameobject_source_ddf_pb2.EmbeddedInstanceDesc())
        self.assertEqual("prototype", instance.WhichOneof("payload"))


if __name__ == "__main__":
    unittest.main()
