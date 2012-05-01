package com.dynamo.cr.luaeditor.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import com.dynamo.cr.luaeditor.LuaContentAssistProcessor;
import com.dynamo.cr.luaeditor.LuaParseResult;

public class LuaContentAssistProcessorTest {

    @Test
    public void testAssist() throws Exception {

        LuaParseResult result;

        result = LuaContentAssistProcessor.parseLine("");
        assertNotNull(result);
        assertEquals("", result.getNamespace());
        assertEquals("", result.getFunction());
        assertTrue(!result.inFunction());
        assertEquals(0, result.getMatchStart());
        assertEquals(0, result.getMatchEnd());

        result = LuaContentAssistProcessor.parseLine("ha");
        assertNotNull(result);
        assertEquals("", result.getNamespace());
        assertEquals("ha", result.getFunction());
        assertTrue(!result.inFunction());
        assertEquals(0, result.getMatchStart());
        assertEquals(2, result.getMatchEnd());

        result = LuaContentAssistProcessor.parseLine("hash(");
        assertNotNull(result);
        assertEquals("", result.getNamespace());
        assertEquals("hash", result.getFunction());
        assertTrue(result.inFunction());
        assertEquals(0, result.getMatchStart());
        assertEquals(4, result.getMatchEnd());

        result = LuaContentAssistProcessor.parseLine("gui.");
        assertEquals("gui", result.getNamespace());
        assertEquals("", result.getFunction());
        assertTrue(!result.inFunction());
        assertEquals(0, result.getMatchStart());
        assertEquals(4, result.getMatchEnd());

        result = LuaContentAssistProcessor.parseLine("gui.animate");
        assertEquals("gui", result.getNamespace());
        assertEquals("animate", result.getFunction());
        assertTrue(!result.inFunction());
        assertEquals(0, result.getMatchStart());
        assertEquals("gui.animate".length(), result.getMatchEnd());

        result = LuaContentAssistProcessor.parseLine("gui.animate(node, property,");
        assertEquals("gui", result.getNamespace());
        assertEquals("animate", result.getFunction());
        assertTrue(result.inFunction());
        assertEquals(0, result.getMatchStart());
        assertEquals("gui.animate".length(), result.getMatchEnd());

        result = LuaContentAssistProcessor.parseLine("gui.EASING_IN");
        assertEquals("gui", result.getNamespace());
        assertEquals("EASING_IN", result.getFunction());
        assertTrue(!result.inFunction());
        assertEquals(0, result.getMatchStart());

        result = LuaContentAssistProcessor.parseLine("gui.animate(node, gui.COL,");
        assertEquals("gui", result.getNamespace());
        assertEquals("COL", result.getFunction());
        assertTrue(!result.inFunction());
        assertEquals(18, result.getMatchStart());

    }
}
