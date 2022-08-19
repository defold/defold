// Copyright 2020-2022 The Defold Foundation
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;

import org.junit.Test;

import com.dynamo.bob.Project;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.test.util.PropertiesTestUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.lua.proto.Lua.LuaModule;
import com.dynamo.script.proto.Lua.LuaSource;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarationEntry;

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
        addFile("/vp.vp", "");
        addFile("/fp.fp", "");
        addFile("/material.material", "name: \"material\"\nvertex_program: \"/vp.vp\"\nfragment_program: \"/fp.fp\"");
        StringBuilder src = new StringBuilder();
        src.append("\n");
        src.append("go.property(\"number\", 1)\n");
        src.append("go.property(\"hash\", hash(\"hash\"))\n");
        src.append("go.property(\"url\", msg.url())\n");
        src.append("go.property(\"vec3\", vmath.vector3(1, 2, 3))\n");
        src.append("go.property(\"vec4\", vmath.vector4(4, 5, 6, 7))\n");
        src.append("go.property(\"quat\", vmath.quat(8, 9, 10, 11))\n");
        src.append("go.property(\"bool\", true)\n");
        src.append("go.property(\"material\", resource.material(\"/material.material\"))\n");
        src.append("\n");
        src.append("    go.property(  \"space_number\"  ,  1   )\n");
        src.append("go.property(\"semi_colon\", 1);\n");
        LuaModule luaModule = (LuaModule)build("/test.script", src.toString()).get(0);
        PropertyDeclarations properties = luaModule.getProperties();
        assertEquals(3, properties.getNumberEntriesCount());
        PropertiesTestUtil.assertNumber(properties, 1, 0);
        PropertiesTestUtil.assertNumber(properties, 1, 1);
        PropertiesTestUtil.assertNumber(properties, 1, 2);
        PropertiesTestUtil.assertHash(properties, MurmurHash.hash64("hash"), 0);
        PropertiesTestUtil.assertHash(properties, MurmurHash.hash64("/material.materialc"), 1);
        PropertiesTestUtil.assertURL(properties, "", 0);
        PropertiesTestUtil.assertVector3(properties, 1, 2, 3, 0);
        PropertiesTestUtil.assertVector4(properties, 4, 5, 6, 7, 0);
        PropertiesTestUtil.assertQuat(properties, 8, 9, 10, 11, 0);
        PropertiesTestUtil.assertBoolean(properties, true, 0);
        assertEquals("/material.materialc", luaModule.getPropertyResources(0));

        // Verify that .x, .y, .z and .w exists as sub element ids for Vec3, Vec4 and Quat.
        assertSubElementsV3(properties.getVector3Entries(0));
        assertSubElementsV4(properties.getVector4Entries(0));
        assertSubElementsV4(properties.getQuatEntries(0));
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
        Project p = GetProject();
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
        Project p = GetProject();
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
        Project p = GetProject();
        p.setOption("platform", "armv7-android");
        p.setOption("architectures", "arm64-android");

        StringBuilder src = new StringBuilder();
        LuaModule luaModule = (LuaModule)build("/test.script", "function foo() print('foo') end").get(0);
        LuaSource luaSource = luaModule.getSource();
        assertTrue(luaSource.getScript().size() == 0);
        assertTrue(luaSource.getBytecode().size() > 0);
        assertTrue(luaSource.getBytecode64().size() == 0);
        assertTrue(luaSource.getDelta().size() == 0);
    }

    @Test
    public void testLuaJITBytecode32WithoutDelta() throws Exception {
        Project p = GetProject();
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
    public void testLuaJITBytecode64WithDelta() throws Exception {
        Project p = GetProject();
        p.setOption("platform", "armv7-android");
        p.setOption("architectures", "armv7-android,arm64-android");

        StringBuilder src = new StringBuilder();
        LuaModule luaModule = (LuaModule)build("/test.script", "function foo() print('foo') end").get(0);
        LuaSource luaSource = luaModule.getSource();
        assertTrue(luaSource.getScript().size() == 0);
        assertTrue(luaSource.getBytecode().size() > 0);
        assertTrue(luaSource.getBytecode64().size() == 0);
        assertTrue(luaSource.getDelta().size() > 0);
    }
}
