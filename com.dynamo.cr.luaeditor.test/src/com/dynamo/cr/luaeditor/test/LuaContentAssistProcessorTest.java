package com.dynamo.cr.luaeditor.test;

import static org.junit.Assert.*;

import org.junit.Test;

import com.dynamo.cr.luaeditor.LuaContentAssistProcessor;
import com.dynamo.cr.luaeditor.LuaParseResult;

public class LuaContentAssistProcessorTest {

    @Test
    public void testAssist() throws Exception {

        LuaParseResult result;

        result = LuaContentAssistProcessor.parseLine("");
        assertEquals(null, result);

        result = LuaContentAssistProcessor.parseLine("gui.");
        assertEquals("gui", result.getNamespace());
        assertEquals("", result.getFunction());
        assertTrue(!result.inFunction());

        result = LuaContentAssistProcessor.parseLine("gui.animate");
        assertEquals("gui", result.getNamespace());
        assertEquals("animate", result.getFunction());
        assertTrue(!result.inFunction());

        result = LuaContentAssistProcessor.parseLine("gui.animate(node, property,");
        assertEquals("gui", result.getNamespace());
        assertEquals("animate", result.getFunction());
        assertTrue(result.inFunction());

        result = LuaContentAssistProcessor.parseLine("gui.EASING_IN");
        assertEquals("gui", result.getNamespace());
        assertEquals("EASING_IN", result.getFunction());
        assertTrue(!result.inFunction());

    }
}
