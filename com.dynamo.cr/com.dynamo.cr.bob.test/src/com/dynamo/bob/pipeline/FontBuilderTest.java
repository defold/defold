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

import org.junit.Before;
import org.junit.Test;

import java.util.List;

import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.render.proto.Font.FontRenderMode;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Font.FontTextureFormat;

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
        ParseUtil.addParser("ttf", content -> null);

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

        String runtimeMaterial = "name: \"runtime_font_material\"\n" +
                                 "tags: \"text\"\n" +
                                 "vertex_program: \"/test.vp\"\n" +
                                 "fragment_program: \"/test.fp\"\n";
        addFile("/builtins/fonts/font-df.material", runtimeMaterial);
        addFile("/builtins/fonts/font-vector-slug.material", runtimeMaterial);
        addFile("/builtins/fonts/font-vector-sweep.material", runtimeMaterial);
        addFile("/builtins/fonts/label-vector-sweep.material", runtimeMaterial);
        addFile("/builtins/fonts/label-vector-sweep-compat.material", runtimeMaterial);
        addFile("/builtins/fonts/label-vector-slug-compat.material", runtimeMaterial);
        addFile("/builtins/fonts/label-df.material", runtimeMaterial);
    }

    @Test
    public void testTTF() throws Exception {

        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        src.append("output_format: TYPE_BITMAP\n");
        src.append("render_mode: MODE_SINGLE_LAYER\n");
        src.append("characters: \"ABC\"\n");
        src.append("all_chars: true\n");
        src.append("shadow_blur: 3\n");
        src.append("shadow_material: \"/test2.material\"\n");

        FontMap fontMap = getFontMap(build("/test.font", src.toString()));
        assertEquals(fontMap.getMaterial(), ResourceUtil.minifyPath("/test.materialc"));
        assertEquals(fontMap.getShadowMaterial(), ResourceUtil.minifyPath("/test2.materialc"));
        assertEquals(FontTextureFormat.TYPE_DISTANCE_FIELD, fontMap.getOutputFormat());
        assertEquals(FontRenderMode.MODE_MULTI_LAYER, fontMap.getRenderMode());
        assertTrue(fontMap.getCharacters().isEmpty());
        assertTrue(!fontMap.getAllChars());
        assertEquals("/Tuffy.ttf", fontMap.getFont());
        assertTrue(fontMap.getGlyphBank().isEmpty());
    }

    @Test
    public void testTTFDefaultShadowMaterial() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        src.append("shadow_blur: 1\n");

        FontMap fontMap = getFontMap(build("/test.font", src.toString()));

        assertEquals(ResourceUtil.minifyPath("/builtins/fonts/font-df.materialc"),
                     fontMap.getShadowMaterial());
    }

    @Test
    public void testTTFZeroBlurUsesSdfEffectMaterial() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        src.append("shadow_blur: 0\n");
        src.append("shadow_alpha: 1\n");
        src.append("shadow_material: \"/test2.material\"\n");

        FontMap fontMap = getFontMap(build("/test.font", src.toString()));

        assertEquals(ResourceUtil.minifyPath("/test2.materialc"),
                     fontMap.getShadowMaterial());
    }

    @Test
    public void testTTFOutlineUsesDefaultSdfEffectMaterial() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        src.append("outline_width: 2\n");
        src.append("shadow_blur: 0\n");

        FontMap fontMap = getFontMap(build("/test.font", src.toString()));

        assertEquals(ResourceUtil.minifyPath("/builtins/fonts/font-df.materialc"),
                     fontMap.getShadowMaterial());
    }

    @Test
    public void testTTFLabelOutlineUsesLabelSdfEffectMaterial() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/builtins/fonts/label-vector-sweep.material\"\n");
        src.append("size: 16\n");
        src.append("outline_width: 2\n");

        FontMap fontMap = getFontMap(build("/test.font", src.toString()));

        assertEquals(ResourceUtil.minifyPath("/builtins/fonts/label-df.materialc"),
                     fontMap.getShadowMaterial());
    }

    @Test
    public void testTTFCompatLabelOutlineUsesLabelSdfEffectMaterial() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/builtins/fonts/label-vector-sweep-compat.material\"\n");
        src.append("size: 16\n");
        src.append("outline_width: 2\n");

        FontMap fontMap = getFontMap(build("/test.font", src.toString()));

        assertEquals(ResourceUtil.minifyPath("/builtins/fonts/label-df.materialc"),
                     fontMap.getShadowMaterial());
    }

    @Test
    public void testFNT() throws Exception {

        StringBuilder src = new StringBuilder();
        src.append("font: \"/bmfont.fnt\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        src.append("output_format: TYPE_DISTANCE_FIELD\n");
        src.append("render_mode: MODE_MULTI_LAYER\n");
        src.append("characters: \"ABC\"\n");
        src.append("shadow_blur: 3\n");
        src.append("shadow_material: \"/test2.material\"\n");
        FontMap fontMap = getFontMap(build("/test.font", src.toString()));

        assertEquals(fontMap.getMaterial(), ResourceUtil.minifyPath("/test.materialc"));
        assertTrue(fontMap.getShadowMaterial().isEmpty());
        assertEquals(FontTextureFormat.TYPE_BITMAP, fontMap.getOutputFormat());
        assertEquals(FontRenderMode.MODE_SINGLE_LAYER, fontMap.getRenderMode());
        assertEquals("ABC", fontMap.getCharacters());
        assertTrue(fontMap.getFont().isEmpty());
        assertTrue(!fontMap.getGlyphBank().isEmpty());
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
