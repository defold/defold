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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import org.junit.Before;
import org.junit.Test;

import java.util.Collections;
import java.util.List;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Progress;
import com.dynamo.bob.Task;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.font.BMFont.BMFontFormatException;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Font.GlyphBank;

import com.google.protobuf.Message;

public class FontBuilderTest extends AbstractProtoBuilderTest {

    static FontMap getFontMap(List<Message> buildResults) {
        for (Message m : buildResults) {
            if (m instanceof FontMap) {
                return (FontMap) m;
            }
        }

        return null;
    }

    @Before
    public void setup() {
        addTestFiles();

        StringBuilder src = new StringBuilder();
        src.append("name: \"test_material\"\n");
        src.append("tags: \"text\"\n");
        src.append("vertex_program: \"/test.vp\"\n");
        src.append("fragment_program: \"/test.fp\"\n");
        addFile("/test.material", src.toString());

        src = new StringBuilder();
        src.append("name: \"test_2_material\"\n");
        src.append("tags: \"test_2\"\n");
        src.append("vertex_program: \"/test2.vp\"\n");
        src.append("fragment_program: \"/test2.fp\"\n");
        addFile("/test2.material", src.toString());
    }

    @Test
    public void testTTF() throws Exception {

        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");

        FontMap fontMap = getFontMap(build("/test.font", src.toString()));
        assertEquals(fontMap.getMaterial(), ResourceUtil.minifyPath("/test.materialc"));
    }

    @Test(timeout = 3000)
    public void testTTFAllCharsBuildPerformance() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        src.append("output_format: TYPE_DISTANCE_FIELD\n");
        src.append("all_chars: true\n");

        List<Message> buildResults = build("/all-chars.font", src.toString());
        FontMap fontMap = getFontMap(buildResults);
        GlyphBank glyphBank = null;
        for (Message message : buildResults) {
            if (message instanceof GlyphBank) {
                glyphBank = (GlyphBank)message;
                break;
            }
        }

        assertTrue(fontMap != null);
        assertTrue(fontMap.getAllChars());
        assertTrue(fontMap.getGlyphBank().endsWith(".glyph_bankc"));
        assertTrue(glyphBank != null);
        assertEquals(1499, glyphBank.getGlyphsCount());
    }

    @Test
    public void testRuntimeGeneratedOTF() throws Exception {
        getProject().setOption("font-runtime-generation", "true");
        addFile("/Test.otf", getFile("/Tuffy.ttf"));

        StringBuilder src = new StringBuilder();
        src.append("font: \"/Test.otf\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        src.append("output_format: TYPE_DISTANCE_FIELD\n");

        addFile("/test.font", src.toString());
        getProject().setInputs(Collections.singletonList("/test.font"));
        List<TaskResult> results = getProject().build(Progress.discarding(), "build");
        assertTrue(results.stream().allMatch(TaskResult::isOk));

        FontMap fontMap = null;
        for (TaskResult result : results) {
            for (IResource output : result.getTask().getOutputs()) {
                if (output.getPath().endsWith(".fontc")) {
                    fontMap = FontMap.parseFrom(output.getContent());
                }
            }
        }
        assertTrue(fontMap != null);
        assertEquals("/Test.otf", fontMap.getFont());
        assertTrue(fontMap.getGlyphBank().isEmpty());
    }

    @Test
    public void testFNT() throws Exception {

        StringBuilder src = new StringBuilder();
        src.append("font: \"/bmfont.fnt\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        FontMap fontMap = getFontMap(build("/test.font", src.toString()));

        assertEquals(fontMap.getMaterial(), ResourceUtil.minifyPath("/test.materialc"));
    }

    @Test
    public void testInvalidFNTReportsCompileException() throws Exception {
        addFile("/invalid.fnt", "invalid");

        StringBuilder src = new StringBuilder();
        src.append("font: \"/invalid.fnt\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        addFile("/invalid.font", src.toString());

        Task task = getProject().createTask(getProject().getResource("/invalid.font"), GlyphBankBuilder.class);
        try {
            task.getBuilder().build(task);
            fail("Expected malformed BMFont data to produce a CompileExceptionError");
        } catch (CompileExceptionError e) {
            assertEquals("invalid.font", e.getResource().getPath());
            assertTrue(e.getCause() instanceof BMFontFormatException);
        }
    }

    @Test
    public void testFNTSubDir() throws Exception {
        byte[] toff_file = getFile("/bmfont.png");
        assertTrue(toff_file != null);
        addFile("/subdir/bmfont.png", toff_file);

        StringBuilder src = new StringBuilder();
        src.append("font: \"/bmfont.fnt\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        FontMap fontMap = getFontMap(build("/subdir/test.font", src.toString()));

        assertEquals(fontMap.getMaterial(), ResourceUtil.minifyPath("/test.materialc"));
    }

    @Test
    public void testFNTGlpyBankPath() throws Exception {

        StringBuilder srcOne = new StringBuilder();
        srcOne.append("font: \"/bmfont.fnt\"\n");
        srcOne.append("material: \"/test.material\"\n");
        srcOne.append("size: 16\n");

        StringBuilder srcTwo = new StringBuilder();
        srcTwo.append("font: \"/bmfont.fnt\"\n");
        srcTwo.append("material: \"/test2.material\"\n");
        srcTwo.append("size: 16\n");

        StringBuilder srcThree = new StringBuilder();
        srcThree.append("font: \"/bmfont.fnt\"\n");
        srcThree.append("material: \"/test2.material\"\n");
        srcThree.append("size: 16\n");
        srcThree.append("shadow_x: 1337.0\n");

        FontMap fontMapOne   = getFontMap(build("/test1.font", srcOne.toString()));
        FontMap fontMapTwo   = getFontMap(build("/test2.font", srcTwo.toString()));
        FontMap fontMapThree = getFontMap(build("/test3.font", srcThree.toString()));

        assertEquals(fontMapOne.getGlyphBank(), fontMapTwo.getGlyphBank());
        assertEquals(fontMapOne.getGlyphBank(), fontMapThree.getGlyphBank());

        assertNotEquals(fontMapOne.getMaterial(), fontMapTwo.getMaterial());
        assertNotEquals(fontMapOne.getShadowX(), fontMapThree.getShadowX());
    }
}
