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

import org.junit.Test;

import com.dynamo.bob.Project;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.test.util.PropertiesTestUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.lua.proto.Lua.LuaModule;
import com.dynamo.script.proto.Lua.LuaSource;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarationEntry;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

public class LuaBuilderTest extends AbstractProtoBuilderTest {

    private static void assertSubElementsV3(PropertyDeclarationEntry entry) {
        assertEquals(MurmurHash.hash64(entry.getKey() + ".x"), entry.getElementIds(0));
        assertEquals(MurmurHash.hash64(entry.getKey() + ".y"), entry.getElementIds(1));
        assertEquals(MurmurHash.hash64(entry.getKey() + ".z"), entry.getElementIds(2));
    }

    private static void assertSubElementsV4(PropertyDeclarationEntry entry) {
        assertEquals(MurmurHash.hash64(entry.getKey() + ".x"), entry.getElementIds(0));
        assertEquals(MurmurHash.hash64(entry.getKey() + ".y"), entry.getElementIds(1));
        assertEquals(MurmurHash.hash64(entry.getKey() + ".z"), entry.getElementIds(2));
        assertEquals(MurmurHash.hash64(entry.getKey() + ".w"), entry.getElementIds(3));
    }

    @Test
    public void testProps() throws Exception {
        addFile("/vp.vp", ShaderProgramBuilderTest.vp);
        addFile("/fp.fp", ShaderProgramBuilderTest.fp);
        addFile("/material.material", "name: \"material\"\nvertex_program: \"/vp.vp\"\nfragment_program: \"/fp.fp\"");
        StringBuilder src = new StringBuilder();
        src.append("\n");
        src.append("go.property(\"number\", 1)\n");
        src.append("go.property(\"hash\", hash(\"hash\"))\n");
        src.append("go.property(\"url\", msg.url())\n");
        src.append("go.property(\"vec3\", vmath.vector3(1, 2, 3))\n");
        src.append("go.property(\"vec3empt\", vmath.vector3())\n");
        src.append("go.property(\"vec3negative\", vmath.vector3(-1))\n");
        src.append("go.property(\"vec4\", vmath.vector4(4, 5, 6, 7))\n");
        src.append("go.property(\"quat\", vmath.quat(8, 9, 10, 11))\n");
        src.append("go.property(\"bool\", true)\n");
        src.append("go.property(\"material\", resource.material(\"/material.material\"))\n");
        src.append("\n");
        src.append("    go.property(  \"space_number\"  ,  1   )\n");
        src.append("go.property(\"semi_colon\", 1);\n");
        LuaModule luaModule = getMessage(build("/test.script", src.toString()), LuaModule.class);
        PropertyDeclarations properties = luaModule.getProperties();
        assertEquals(3, properties.getNumberEntriesCount());
        PropertiesTestUtil.assertNumber(properties, 1, 0);
        PropertiesTestUtil.assertNumber(properties, 1, 1);
        PropertiesTestUtil.assertNumber(properties, 1, 2);
        PropertiesTestUtil.assertHash(properties, MurmurHash.hash64("hash"), 0);
        PropertiesTestUtil.assertHash(properties, MurmurHash.hash64("/material.materialc"), 1);
        PropertiesTestUtil.assertURL(properties, "", 0);
        PropertiesTestUtil.assertVector3(properties, 1, 2, 3, 0);
        PropertiesTestUtil.assertVector3(properties, 0, 0, 0, 1);
        PropertiesTestUtil.assertVector3(properties, -1, -1, -1, 2);
        PropertiesTestUtil.assertVector4(properties, 4, 5, 6, 7, 0);
        PropertiesTestUtil.assertQuat(properties, 8, 9, 10, 11, 0);
        PropertiesTestUtil.assertBoolean(properties, true, 0);
        assertEquals("/material.materialc", luaModule.getPropertyResources(0));

        // Verify that .x, .y, .z and .w exists as sub element ids for Vec3, Vec4 and Quat.
        assertSubElementsV3(properties.getVector3Entries(0));
        assertSubElementsV4(properties.getVector4Entries(0));
        assertSubElementsV4(properties.getQuatEntries(0));
    }

    // DEF-7341 - using resource.xxx(path) for resources that doesn't exist should raise an error
    @Test
    public void testPropResourceNotFound() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("\n");
        src.append("go.property(\"material\", resource.material(\"/invalid.material\"))\n");

        try {
            @SuppressWarnings("unused")
            LuaModule luaModule = (LuaModule) build("/test.script", src.toString()).get(0);
            assertTrue(false);
        } catch (CompileExceptionError e) { }
    }

    @Test
    public void testPropUnsupportedType() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("\n");
        src.append("go.property(\"string\", \"\")\n");
        try {
            @SuppressWarnings("unused")
            LuaModule luaModule = (LuaModule)build("/test.script", src.toString()).get(0);
            assertTrue(false);
        } catch (CompileExceptionError e) {
            assertEquals(2, e.getLineNumber());
        }
    }

    @Test
    public void testUseUncompressedLuaSource() throws Exception {
        Project p = getProject();
        p.setOption("use-uncompressed-lua-source", "true");

        final String path = "/test.script";
        final String scriptSource = "function foo() print('foo') end";
        LuaModule luaModule = (LuaModule)build(path, scriptSource).get(0);
        LuaSource luaSource = luaModule.getSource();
        assertTrue(luaSource.getScript() != null);
        assertTrue(luaSource.getScript().size() > 0);
        assertTrue(luaSource.getBytecode().size() == 0);
        assertTrue(luaSource.getBytecode64().size() == 0);
        assertTrue(luaSource.getDelta().size() == 0);
        assertTrue(p.getOutputFlags("build" + path + "c").contains(Project.OutputFlags.UNCOMPRESSED));
    }

    @Test
    public void testCompressedLuaSourceForHTML5() throws Exception {
        Project p = getProject();
        p.setOption("platform", "js-web");

        final String path = "/test.script";
        final String scriptSource = "function foo() print('foo') end";
        LuaModule luaModule = (LuaModule)build(path, scriptSource).get(0);
        LuaSource luaSource = luaModule.getSource();
        assertTrue(luaSource.getScript() != null);
        assertTrue(luaSource.getScript().size() > 0);
        assertTrue(luaSource.getBytecode().size() == 0);
        assertTrue(luaSource.getBytecode64().size() == 0);
        assertTrue(luaSource.getDelta().size() == 0);
        assertTrue(p.getOutputFlags("build" + path + "c").contains(Project.OutputFlags.ENCRYPTED));
        assertFalse(p.getOutputFlags("build" + path + "c").contains(Project.OutputFlags.UNCOMPRESSED));
    }

    // Leaving these tests in case we decide to reintroduce bytecode generation for Lua 5.1.5
    //
    // @Test
    // public void testVanillaLuaBytecode() throws Exception {
    //     Project p = GetProject();
    //     p.setOption("platform", "js-web");

    //     StringBuilder src = new StringBuilder();
    //     LuaModule luaModule = (LuaModule)build("/test.script", "function foo() print('foo') end").get(0);
    //     LuaSource luaSource = luaModule.getSource();
    //     assertTrue(luaSource.getScript().size() == 0);
    //     assertTrue(luaSource.getBytecode().size() > 0);
    //     assertTrue(luaSource.getBytecode64().size() == 0);
    //     assertTrue(luaSource.getDelta().size() == 0);
    // }

    // @Test
    // public void testVanillaLuaBytecodeChunkname() throws Exception {
    //     Project p = GetProject();
    //     p.setOption("platform", "js-web");

    //     StringBuilder src = new StringBuilder();
    //     LuaModule luaModule = (LuaModule)build("/test.script", "function foo() print('foo') end").get(0);
    //     LuaSource luaSource = luaModule.getSource();
    //     String chunkname = luaSource.getBytecode().substring(12+4, 12+4+12).toStringUtf8();
    //     assertEquals("@test.script", chunkname);
    // }

    @Test
    public void testLuaJITBytecode64WithoutDelta() throws Exception {
        Project p = getProject();
        p.setOption("platform", "armv7-android");
        p.setOption("architectures", "arm64-android");

        LuaModule luaModule = (LuaModule)build("/test.script", "function foo() print('foo') end").get(0);
        LuaSource luaSource = luaModule.getSource();
        assertTrue(luaSource.getScript().size() == 0);
        assertTrue(luaSource.getBytecode().size() > 0);
        assertTrue(luaSource.getBytecode64().size() == 0);
        assertTrue(luaSource.getDelta().size() == 0);
    }

    @Test
    public void testLuaJITBytecode32WithoutDelta() throws Exception {
        Project p = getProject();
        p.setOption("platform", "armv7-android");
        p.setOption("architectures", "armv7-android");

        StringBuilder src = new StringBuilder();
        LuaModule luaModule = (LuaModule)build("/test.script", "function foo() print('foo') end").get(0);
        LuaSource luaSource = luaModule.getSource();
        assertTrue(luaSource.getScript().size() == 0);
        assertTrue(luaSource.getBytecode().size() > 0);
        assertTrue(luaSource.getBytecode64().size() == 0);
        assertTrue(luaSource.getDelta().size() == 0);
    }

    @Test
    public void testLuaJITBytecode32And64WithoutDelta() throws Exception {
        Project p = getProject();
        p.setOption("platform", "armv7-android");
        p.setOption("architectures", "armv7-android,arm64-android");

        LuaModule luaModule = (LuaModule)build("/test.script", "function foo() print('foo') end").get(0);
        LuaSource luaSource = luaModule.getSource();
        assertTrue(luaSource.getScript().size() == 0);
        assertTrue(luaSource.getBytecode32().size() > 0);
        assertTrue(luaSource.getBytecode64().size() > 0);
        assertTrue(luaSource.getDelta().size() == 0);
    }

    @Test
    public void testLuaJITBytecode64WithDelta() throws Exception {
        Project p = getProject();
        p.setOption("platform", "armv7-android");
        p.setOption("architectures", "armv7-android,arm64-android");
        p.setOption("use-lua-bytecode-delta", "true");

        StringBuilder src = new StringBuilder();
        LuaModule luaModule = (LuaModule)build("/test.script", "function foo() print('foo') end").get(0);
        LuaSource luaSource = luaModule.getSource();
        assertTrue(luaSource.getScript().size() == 0);
        assertTrue(luaSource.getBytecode().size() > 0);
        assertTrue(luaSource.getBytecode64().size() == 0);
        assertTrue(luaSource.getDelta().size() > 0);
    }

    @Test
    public void testLuaBytecodeDeltaCalculation() throws Exception {
        LuaBuilder builder = new LuaBuilder() {};

        // create some mock bytecode
        byte[] b32 = new byte[258];
        byte[] b64 = new byte[258];
        // byte 0 is the same
        b32[0] = 5;
        b64[0] = 5;
        // bytes 1 to 256 are diffing
        for (int i=1; i <= 256; i++) {
            b32[i] = 99;
            b64[i] = 66;
        }
        // byte 257 is the same
        b32[257] = 11;
        b64[257] = 11;

        // construct the delta
        byte[] delta = builder.constructBytecodeDelta(b64, b32);

        // each diff consists of index (1-4 bytes), count (1 byte), bytes (count bytes)
        // this means that each diff contains a maximum of 255 bytes

        // the bytecode in this test is larger than 256 bytes byte but less than
        // 65536 bytes which means that the index will use two bytes

        // first diff is on byte 1-255
        // index - diff is on the 2nd byte (0x0001)
        assertTrue(delta[0] == 0x01);
        assertTrue(delta[1] == 0x00);
        // count - diff consists of 255 values
        assertTrue((delta[2] & 0xff) == 255);
        // byte - diffing bytes
        for (int i = 1; i <= 255; i++) {
            assertTrue(delta[2 + i] == 99);
        }

        // second diff is on byte 256
        // index - diff is on the 256th byte (0x0100)
        assertTrue(delta[258] == 0x00);
        assertTrue(delta[259] == 0x01);
        // count - diff consists of 1 value
        assertTrue((delta[260] & 0xff) == 1);
        // byte - the last diffing byte
        assertTrue(delta[261] == 99);
    }

    @Test
    public void testCircularRequire() throws Exception {
        addFile("/first.lua", "require(\"second\")");
        addFile("/second.lua", "require(\"third\")");
        addFile("/third.lua", "require(\"first\")");
        try {
            build("/main.lua", "require(\"first\")");
            fail("Expected a CompileExceptionError due to circular dependency, but no exception was thrown.");
        }
        catch (CompileExceptionError e) {
            assertTrue(e.getMessage().contains("Circular dependency detected"));
        }
    }
}
