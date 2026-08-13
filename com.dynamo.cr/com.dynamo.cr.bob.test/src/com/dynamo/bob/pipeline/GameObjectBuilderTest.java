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

import java.nio.charset.StandardCharsets;
import java.util.List;

import org.junit.Assert;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.test.util.PropertiesTestUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.gameobject.proto.GameObjectSource;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gamesys.proto.DataProto.Data;
import com.dynamo.gamesys.proto.MeshProto.MeshDesc;
import com.dynamo.gamesys.proto.Sound.SoundDesc;
import com.dynamo.gamesys.proto.Sprite.SpriteDesc;
import com.dynamo.lua.proto.Lua.LuaModule;
import com.dynamo.particle.proto.Particle.ParticleFX;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;
import com.dynamo.proto.DdfStruct.Struct;
import com.dynamo.proto.DdfStruct.Value;
import com.google.protobuf.Message;

public class GameObjectBuilderTest extends AbstractProtoBuilderTest {

    private static String escapeProtobufString(String value) {
        return value.replace("\\", "\\\\")
                .replace("\"", "\\\"")
                .replace("\r", "\\r")
                .replace("\n", "\\n");
    }

    private static PrototypeDesc normalizeGeneratedComponentPaths(PrototypeDesc prototype) {
        PrototypeDesc.Builder builder = prototype.toBuilder();
        for (int i = 0; i < builder.getComponentsCount(); ++i) {
            builder.setComponents(i, builder.getComponents(i).toBuilder().setComponent("<generated>"));
        }
        return builder.build();
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
        src.append("  properties { id: \"text\" value: \"hello\" type: PROPERTY_TYPE_TEXT }\n");
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
            PropertiesTestUtil.assertText(properties, "hello", 0);
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

    // Verifies that legacy string and typed sprite payloads produce equivalent compiled game objects.
    @Test
    public void testLegacyAndTypedEmbeddedComponentsBuildEquivalently() throws Exception {
        addFile("/test.atlas", "");
        addFile("build/test.a.texturesetc", "DUMMY_DATA");

        String animation = "Spelare åäö \ud83d\ude00";
        String spriteData = "tile_set: \"/test.atlas\"\n"
                + "default_animation: \"" + animation + "\"\n"
                + "material: \"\"\n";

        String legacySource = "embedded_components {\n"
                + "  id: \"sprite\"\n"
                + "  type: \"sprite\"\n"
                + "  data: \"" + escapeProtobufString(spriteData) + "\"\n"
                + "}\n";
        String typedSource = "embedded_components {\n"
                + "  id: \"sprite\"\n"
                + "  type: \"sprite\"\n"
                + "  sprite {\n"
                + "    tile_set: \"/test.atlas\"\n"
                + "    default_animation: \"" + animation + "\"\n"
                + "    material: \"\"\n"
                + "  }\n"
                + "}\n";
        List<Message> typedMessages = build("/typed.go", typedSource);
        PrototypeDesc typedPrototype = getMessage(typedMessages, PrototypeDesc.class);
        SpriteDesc typedSprite = getMessage(typedMessages, SpriteDesc.class);

        PrototypeDesc legacyPrototype = getMessage(build("/legacy.go", legacySource), PrototypeDesc.class);

        Assert.assertEquals(normalizeGeneratedComponentPaths(legacyPrototype),
                normalizeGeneratedComponentPaths(typedPrototype));
        Assert.assertEquals(0, typedPrototype.getEmbeddedComponentsCount());
        Assert.assertEquals(1, typedPrototype.getComponentsCount());
        Assert.assertNotNull(typedSprite);
        Assert.assertEquals(animation, typedSprite.getDefaultAnimation());
    }

    // Verifies deterministic typed-payload identities and unchanged UTF-8 identities for legacy payloads.
    @Test
    public void testTypedGeneratedInputUsesDeterministicBinaryIdentity() throws Exception {
        Value one = Value.newBuilder().setNumber(1.0).build();
        Value two = Value.newBuilder().setNumber(2.0).build();
        Struct firstStruct = Struct.newBuilder()
                .putFields("b", two)
                .putFields("a", one)
                .build();
        Struct secondStruct = Struct.newBuilder()
                .putFields("a", one)
                .putFields("b", two)
                .build();

        GameObjectSource.EmbeddedComponentDesc first = GameObjectSource.EmbeddedComponentDesc.newBuilder()
                .setId("light")
                .setType("point_light")
                .setPointLight(Data.newBuilder().setData(Value.newBuilder().setStruct(firstStruct)))
                .build();
        GameObjectSource.EmbeddedComponentDesc second = GameObjectSource.EmbeddedComponentDesc.newBuilder()
                .setId("light")
                .setType("point_light")
                .setPointLight(Data.newBuilder().setData(Value.newBuilder().setStruct(secondStruct)))
                .build();

        GameObjectSourceUtil.GeneratedInput firstInput = GameObjectSourceUtil.getEmbeddedComponentInput(
                getProject().getResource("/test.go"), first);
        GameObjectSourceUtil.GeneratedInput secondInput = GameObjectSourceUtil.getEmbeddedComponentInput(
                getProject().getResource("/test.go"), second);

        Assert.assertTrue(firstInput.isTyped());
        Assert.assertSame(first.getPointLight(), firstInput.getMessage());
        Assert.assertArrayEquals(firstInput.getContent(), secondInput.getContent());
        Assert.assertEquals(0, firstInput.getContent()[0]);

        String legacyText = "data { number: 1.0 }\n";
        GameObjectSource.EmbeddedComponentDesc legacy = GameObjectSource.EmbeddedComponentDesc.newBuilder()
                .setId("light")
                .setType("point_light")
                .setData(legacyText)
                .build();
        GameObjectSourceUtil.GeneratedInput legacyInput = GameObjectSourceUtil.getEmbeddedComponentInput(
                getProject().getResource("/test.go"), legacy);

        Assert.assertFalse(legacyInput.isTyped());
        Assert.assertArrayEquals(legacyText.getBytes(StandardCharsets.UTF_8), legacyInput.getContent());
    }

    // Verifies that a typed mesh payload is passed directly to its builder and its resources are compiled.
    @Test
    public void testTypedEmbeddedMeshUsesInjectedSourceMessage() throws Exception {
        addTestFiles();
        addFile("/test.buffer", "[]");
        addFile("/test.material", "name: \"test\"\n"
                + "vertex_program: \"/test.vp\"\n"
                + "fragment_program: \"/test.fp\"\n");
        String source = "embedded_components {\n"
                + "  id: \"mesh\"\n"
                + "  type: \"mesh\"\n"
                + "  mesh {\n"
                + "    material: \"/test.material\"\n"
                + "    vertices: \"/test.buffer\"\n"
                + "  }\n"
                + "}\n";

        List<Message> messages = build("/typed-mesh.go", source);
        PrototypeDesc prototype = getMessage(messages, PrototypeDesc.class);
        MeshDesc mesh = getMessage(messages, MeshDesc.class);

        Assert.assertEquals(1, prototype.getComponentsCount());
        Assert.assertNotNull(mesh);
        Assert.assertEquals("/test.bufferc", mesh.getVertices());
        Assert.assertEquals("/test.materialc", mesh.getMaterial());
    }

    // Verifies that ordinary singular message fragments retain protobuf text merge semantics.
    @Test
    public void testSingularMessageCanBeSplitAcrossFragments() throws Exception {
        String source = "embedded_components {\n"
                + "  id: \"particlefx\"\n"
                + "  type: \"particlefx\"\n"
                + "  position { x: 1.0 }\n"
                + "  position { y: 2.0 }\n"
                + "  particlefx {}\n"
                + "}\n";

        PrototypeDesc prototype = getMessage(build("/split-message.go", source), PrototypeDesc.class);

        Assert.assertEquals(1, prototype.getComponentsCount());
        Assert.assertEquals(1.0f, prototype.getComponents(0).getPosition().getX(), 0.0f);
        Assert.assertEquals(2.0f, prototype.getComponents(0).getPosition().getY(), 0.0f);
    }

    // Verifies that a referenced raw audio file is converted into exactly one typed sound component.
    @Test
    public void testRawAudioComponentBecomesOneTypedSound() throws Exception {
        addTestFiles();
        String source = "components { id: \"sound\" component: \"/test.ogg\" }\n";

        List<Message> messages = build("/raw-audio.go", source);
        PrototypeDesc prototype = getMessage(messages, PrototypeDesc.class);
        SoundDesc sound = getMessage(messages, SoundDesc.class);

        Assert.assertEquals(0, prototype.getEmbeddedComponentsCount());
        Assert.assertEquals(1, prototype.getComponentsCount());
        Assert.assertEquals("sound", prototype.getComponents(0).getId());
        Assert.assertTrue(prototype.getComponents(0).getComponent().endsWith(".soundc"));
        Assert.assertNotNull(sound);
        Assert.assertEquals("/test.oggc", sound.getSound());
    }

    // Verifies that legacy fallback payloads preserve non-ASCII text through compilation.
    @Test
    public void testFallbackEmbeddedComponentPreservesUtf8() throws Exception {
        getProject().setOption("use-uncompressed-lua-source", "true");
        String script = "print(\"Spelare åäö \ud83d\ude00\")\n";
        String source = "embedded_components {\n"
                + "  id: \"script\"\n"
                + "  type: \"script\"\n"
                + "  data: \"" + escapeProtobufString(script) + "\"\n"
                + "}\n";

        List<Message> messages = build("/utf8.go", source);
        PrototypeDesc prototype = getMessage(messages, PrototypeDesc.class);
        LuaModule luaModule = getMessage(messages, LuaModule.class);

        Assert.assertEquals(0, prototype.getEmbeddedComponentsCount());
        Assert.assertEquals(1, prototype.getComponentsCount());
        Assert.assertNotNull(luaModule);
        Assert.assertArrayEquals(script.getBytes(StandardCharsets.UTF_8), luaModule.getSource().getScript().toByteArray());
    }

    // Verifies that equal payload hashes from different component types create distinct generated resources.
    @Test
    public void testEmbeddedComponentsWithSamePayloadHashAndDifferentTypes() throws Exception {
        String source = "embedded_components {\n"
                + "  id: \"particlefx\"\n"
                + "  type: \"particlefx\"\n"
                + "  particlefx {}\n"
                + "}\n"
                + "embedded_components {\n"
                + "  id: \"script\"\n"
                + "  type: \"script\"\n"
                + "  data: \"\"\n"
                + "}\n";

        List<Message> messages = build("/same-payload.go", source);
        PrototypeDesc prototype = getMessage(messages, PrototypeDesc.class);

        Assert.assertEquals(0, prototype.getEmbeddedComponentsCount());
        Assert.assertEquals(2, prototype.getComponentsCount());
        Assert.assertNotEquals(prototype.getComponents(0).getComponent(), prototype.getComponents(1).getComponent());
        Assert.assertNotNull(getMessage(messages, ParticleFX.class));
        Assert.assertNotNull(getMessage(messages, LuaModule.class));
    }

    // Verifies that an embedded component without any payload is rejected.
    @Test(expected = CompileExceptionError.class)
    public void testEmbeddedComponentMissingPayload() throws Exception {
        build("/missing-payload.go", "embedded_components { id: \"fx\" type: \"particlefx\" }");
    }

    // Verifies that a typed payload whose arm differs from the component type is rejected.
    @Test(expected = CompileExceptionError.class)
    public void testEmbeddedComponentPayloadTypeMismatch() throws Exception {
        build("/mismatched-payload.go",
                "embedded_components { id: \"fx\" type: \"sprite\" particlefx {} }");
    }

    // Verifies that source text selecting conflicting embedded-component payload arms is rejected.
    @Test(expected = CompileExceptionError.class)
    public void testEmbeddedComponentMultiplePayloads() throws Exception {
        build("/multiple-payloads.go",
                "embedded_components { id: \"fx\" type: \"particlefx\" data: \"\" particlefx {} }");
    }

    // Verifies that missing and mismatched component payload errors identify the invalid component.
    @Test
    public void testEmbeddedComponentValidationMessages() throws Exception {
        try {
            build("/missing-payload-message.go",
                    "embedded_components { id: \"fx\" type: \"particlefx\" }");
            Assert.fail("Expected missing payload rejection");
        } catch (CompileExceptionError e) {
            Assert.assertTrue(e.getMessage().contains("Embedded component 'fx' is missing a payload"));
        }

        try {
            build("/mismatched-payload-message.go",
                    "embedded_components { id: \"fx\" type: \"sprite\" particlefx {} }");
            Assert.fail("Expected mismatched payload rejection");
        } catch (CompileExceptionError e) {
            Assert.assertTrue(e.getMessage().contains(
                    "Embedded component 'fx' has type 'sprite' but uses payload 'particlefx'"));
        }
    }
}
