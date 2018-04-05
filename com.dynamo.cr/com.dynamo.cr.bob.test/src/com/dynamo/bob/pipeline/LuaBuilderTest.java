package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.test.util.PropertiesTestUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.lua.proto.Lua.LuaModule;
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
        StringBuilder src = new StringBuilder();
        src.append("\n");
        src.append("go.property(\"number\", 1)\n");
        src.append("go.property(\"hash\", hash(\"hash\"))\n");
        src.append("go.property(\"url\", msg.url())\n");
        src.append("go.property(\"vec3\", vmath.vector3(1, 2, 3))\n");
        src.append("go.property(\"vec4\", vmath.vector4(4, 5, 6, 7))\n");
        src.append("go.property(\"quat\", vmath.quat(8, 9, 10, 11))\n");
        src.append("go.property(\"bool\", true)\n");
        src.append("\n");
        src.append("    go.property(  \"space_number\"  ,  1   )\n");
        src.append("go.property(\"semi_colon\", 1); \n");
        LuaModule luaModule = (LuaModule)build("/test.script", src.toString()).get(0);
        PropertyDeclarations properties = luaModule.getProperties();
        assertEquals(3, properties.getNumberEntriesCount());
        PropertiesTestUtil.assertNumber(properties, 1, 0);
        PropertiesTestUtil.assertNumber(properties, 1, 1);
        PropertiesTestUtil.assertNumber(properties, 1, 2);
        PropertiesTestUtil.assertHash(properties, MurmurHash.hash64("hash"), 0);
        PropertiesTestUtil.assertURL(properties, "", 0);
        PropertiesTestUtil.assertVector3(properties, 1, 2, 3, 0);
        PropertiesTestUtil.assertVector4(properties, 4, 5, 6, 7, 0);
        PropertiesTestUtil.assertQuat(properties, 8, 9, 10, 11, 0);
        PropertiesTestUtil.assertBoolean(properties, true, 0);

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
}
