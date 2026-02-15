// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.pipeline;

import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.List;

import org.junit.Assert;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.test.util.PropertiesTestUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gamesys.proto.Camera.CameraDesc;
import com.dynamo.gamesys.proto.GameSystem.CollectionFactoryDesc;
import com.dynamo.gamesys.proto.GameSystem.CollectionProxyDesc;
import com.dynamo.gamesys.proto.GameSystem.FactoryDesc;
import com.dynamo.gamesys.proto.Label.LabelDesc;
import com.dynamo.gamesys.proto.ModelProto.ModelDesc;
import com.dynamo.gamesys.proto.Physics.CollisionObjectDesc;
import com.dynamo.gamesys.proto.Sound.SoundDesc;
import com.dynamo.gamesys.proto.Sprite.SpriteDesc;
import com.dynamo.particle.proto.Particle.ParticleFX;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;
import com.google.protobuf.TextFormat;

public class GameObjectBuilderTest extends AbstractProtoBuilderTest {

    private static final List<String> TYPED_COMPONENT_TYPES = Arrays.asList(
        "collisionobject",
        "label",
        "sprite",
        "sound",
        "model",
        "factory",
        "collectionfactory",
        "particlefx",
        "camera",
        "collectionproxy");

    private static EmbeddedComponentDesc makeTypedEmbeddedComponent(String type) {
        EmbeddedComponentDesc.Builder builder = EmbeddedComponentDesc.newBuilder()
            .setId(type)
            .setType(type);

        switch (type) {
            case "collisionobject":
                return builder.setCollisionobject(CollisionObjectDesc.newBuilder().buildPartial()).buildPartial();
            case "label":
                return builder.setLabel(LabelDesc.newBuilder().buildPartial()).buildPartial();
            case "sprite":
                return builder.setSprite(SpriteDesc.newBuilder().buildPartial()).buildPartial();
            case "sound":
                return builder.setSound(SoundDesc.newBuilder().buildPartial()).buildPartial();
            case "model":
                return builder.setModel(ModelDesc.newBuilder().buildPartial()).buildPartial();
            case "factory":
                return builder.setFactory(FactoryDesc.newBuilder().buildPartial()).buildPartial();
            case "collectionfactory":
                return builder.setCollectionfactory(CollectionFactoryDesc.newBuilder().buildPartial()).buildPartial();
            case "particlefx":
                return builder.setParticlefx(ParticleFX.newBuilder().buildPartial()).buildPartial();
            case "camera":
                return builder.setCamera(CameraDesc.newBuilder().buildPartial()).buildPartial();
            case "collectionproxy":
                return builder.setCollectionproxy(CollectionProxyDesc.newBuilder().buildPartial()).buildPartial();
            default:
                throw new IllegalArgumentException("Unsupported type: " + type);
        }
    }

    private static String typedPayloadToLegacyData(EmbeddedComponentDesc typed) {
        switch (typed.getPayloadCase()) {
            case COLLISIONOBJECT:
                return TextFormat.printToString(typed.getCollisionobject());
            case LABEL:
                return TextFormat.printToString(typed.getLabel());
            case SPRITE:
                return TextFormat.printToString(typed.getSprite());
            case SOUND:
                return TextFormat.printToString(typed.getSound());
            case MODEL:
                return TextFormat.printToString(typed.getModel());
            case FACTORY:
                return TextFormat.printToString(typed.getFactory());
            case COLLECTIONFACTORY:
                return TextFormat.printToString(typed.getCollectionfactory());
            case PARTICLEFX:
                return TextFormat.printToString(typed.getParticlefx());
            case CAMERA:
                return TextFormat.printToString(typed.getCamera());
            case COLLECTIONPROXY:
                return TextFormat.printToString(typed.getCollectionproxy());
            default:
                throw new IllegalArgumentException("Unexpected payload case: " + typed.getPayloadCase());
        }
    }

    @Test
    public void testTypedEmbeddedComponentPayloadProducesSameTypeAndBytesAsLegacyDataForAllBuiltins() throws Exception {

        Method typeMethod = GameObjectBuilder.class.getDeclaredMethod("embeddedComponentType", EmbeddedComponentDesc.class);
        typeMethod.setAccessible(true);
        Method dataMethod = GameObjectBuilder.class.getDeclaredMethod("embeddedComponentDataBytes", EmbeddedComponentDesc.class);
        dataMethod.setAccessible(true);

        for (String type : TYPED_COMPONENT_TYPES) {
            EmbeddedComponentDesc typed = makeTypedEmbeddedComponent(type);
            EmbeddedComponentDesc legacy = EmbeddedComponentDesc.newBuilder()
                .setId(type)
                .setType(type)
                .setData(typedPayloadToLegacyData(typed))
                .buildPartial();

            Assert.assertEquals(type, typeMethod.invoke(null, typed));
            Assert.assertEquals(typeMethod.invoke(null, legacy), typeMethod.invoke(null, typed));
            Assert.assertArrayEquals((byte[]) dataMethod.invoke(null, legacy), (byte[]) dataMethod.invoke(null, typed));
        }
    }

    @Test
    public void testUnknownEmbeddedComponentTypeFallsBackToLegacyFields() throws Exception {
        EmbeddedComponentDesc desc = EmbeddedComponentDesc.newBuilder()
            .setId("custom")
            .setType("custom")
            .setData("value: 1\n")
            .build();

        Method typeMethod = GameObjectBuilder.class.getDeclaredMethod("embeddedComponentType", EmbeddedComponentDesc.class);
        typeMethod.setAccessible(true);
        Method dataMethod = GameObjectBuilder.class.getDeclaredMethod("embeddedComponentDataBytes", EmbeddedComponentDesc.class);
        dataMethod.setAccessible(true);

        Assert.assertEquals("custom", typeMethod.invoke(null, desc));
        Assert.assertArrayEquals("value: 1\n".getBytes(StandardCharsets.UTF_8), (byte[]) dataMethod.invoke(null, desc));
    }

    @Test
    public void testProps() throws Exception {
        addFile("/test.script", "");
        StringBuilder src = new StringBuilder();
        src.append("components {");
        src.append("  id: \"script\"\n");
        src.append("  component: \"/test.script\"\n");
        src.append("  properties { id: \"number\" value: \"1\" type: PROPERTY_TYPE_NUMBER }\n");
        src.append("  properties { id: \"hash\" value: \"hash\" type: PROPERTY_TYPE_HASH }\n");
        src.append("  properties { id: \"url\" value: \"url\" type: PROPERTY_TYPE_URL }\n");
        src.append("  properties { id: \"vec3\" value: \"1, 2, 3\" type: PROPERTY_TYPE_VECTOR3 }\n");
        src.append("  properties { id: \"vec4\" value: \"4, 5, 6, 7\" type: PROPERTY_TYPE_VECTOR4 }\n");
        src.append("  properties { id: \"quat\" value: \"8, 9, 10, 11\" type: PROPERTY_TYPE_QUAT }\n");
        src.append("  properties { id: \"bool\" value: \"true\" type: PROPERTY_TYPE_BOOLEAN }\n");
        src.append("}\n");
        PrototypeDesc prototype = getMessage(build("/test.go", src.toString()), PrototypeDesc.class);
        for (ComponentDesc cd : prototype.getComponentsList()) {
            PropertyDeclarations properties = cd.getPropertyDecls();
            PropertiesTestUtil.assertNumber(properties, 1, 0);
            PropertiesTestUtil.assertHash(properties, MurmurHash.hash64("hash"), 0);
            PropertiesTestUtil.assertURL(properties, "url", 0);
            PropertiesTestUtil.assertVector3(properties, 1, 2, 3, 0);
            PropertiesTestUtil.assertVector4(properties, 4, 5, 6, 7, 0);
            PropertiesTestUtil.assertQuat(properties, 8, 9, 10, 11, 0);
            PropertiesTestUtil.assertBoolean(properties, true, 0);
        }
    }

    @Test(expected = CompileExceptionError.class)
    public void testPropInvalidValue() throws Exception {
        addFile("/test.script", "");
        StringBuilder src = new StringBuilder();
        src.append("components {");
        src.append("  id: \"script\"\n");
        src.append("  component: \"/test.script\"\n");
        src.append("  properties { id: \"number\" value: \"a\" type: PROPERTY_TYPE_NUMBER }\n");
        src.append("}\n");
        @SuppressWarnings("unused")
        PrototypeDesc prototype = (PrototypeDesc)build("/test.go", src.toString()).get(0);
    }
}
