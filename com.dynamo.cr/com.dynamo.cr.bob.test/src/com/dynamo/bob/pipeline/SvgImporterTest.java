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

import static org.junit.Assert.*;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.slug.SlugCompiler;
import com.dynamo.gamesys.proto.AtlasProto.Atlas;
import com.dynamo.gamesys.proto.AtlasProto.AtlasImage;
import com.dynamo.gamesys.proto.AtlasProto.AtlasAnimation;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSetMode;
import com.dynamo.gamesys.proto.Tile.SpriteTrimmingMode;
import com.dynamo.gamesys.proto.Tile.Playback;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Map;
import org.junit.Test;

public class SvgImporterTest {
    private static byte[] svg(String body) {
        return ("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 50'>" + body + "</svg>").getBytes(StandardCharsets.UTF_8);
    }

    private static void invalid(String body, String message) throws Exception {
        try {
            SvgImporter.parse(svg(body));
            fail("Expected an error for " + body);
        } catch (IOException e) {
            assertTrue(e.getMessage(), e.getMessage().contains(message));
        }
    }

    // Checks viewport fitting and absolute units, guarding against stretched SVGs.
    @Test public void dimensionsAndAspectRatio() throws Exception {
        var image = SvgImporter.parse(("<svg xmlns='http://www.w3.org/2000/svg' width='2in' height='192px' viewBox='10 20 100 50'>" +
                "<rect x='10' y='20' width='100' height='50'/></svg>").getBytes(StandardCharsets.UTF_8));
        assertEquals(192, image.width(), 0);
        assertEquals(192, image.height(), 0);
        var bounds = image.layers().get(0).path().getBounds2D();
        assertEquals(0, bounds.getX(), .00001);
        assertEquals(48, bounds.getY(), .00001);
        assertEquals(192, bounds.getWidth(), .00001);
        assertEquals(96, bounds.getHeight(), .00001);
    }

    // Covers all supported shape families and relative, smooth, and elliptical path commands.
    @Test public void shapesAndPathCommands() throws Exception {
        var image = SvgImporter.parse(svg("<g transform='translate(2 3) scale(2)'>" +
                "<path d='M1 1h5v5l-5 0z M10 10q5-5 10 0t10 0c2 3 4 3 6 0s4-3 6 0a4 3 30 0110 5z'/>" +
                "<rect x='1' y='2' width='8' height='10' rx='2'/><circle cx='20' cy='20' r='5'/>" +
                "<ellipse cx='30' cy='20' rx='5' ry='8'/><polygon points='1,1 8,1 4,7'/><polyline points='1 1 8 1 4 7'/></g>"));
        assertEquals(6, image.layers().size());
        var compiled = SlugCompiler.compileAtlas(new SlugCompiler.Image[]{SvgImporter.toVectorImage(image)});
        assertEquals(4096, compiled.curves().width);
        assertTrue(compiled.curves().pixels.remaining() > 0);
    }

    // A drawing command after closepath starts a new contour at the closed path's origin.
    @Test
    public void drawingAfterClosePath() throws Exception {
        var image = SvgImporter.parse(svg("<path d='M10 10H30V30Z h-5v-5z'/>"));
        var bounds = image.layers().get(0).path().getBounds2D();
        assertEquals(5, bounds.getX(), 0);
        assertEquals(5, bounds.getY(), 0);
        var compiled = SlugCompiler.compileAtlas(new SlugCompiler.Image[]{SvgImporter.toVectorImage(image)});
        assertTrue(compiled.curves().pixels.remaining() > 0);
    }

    // Preserves inherited fill/style precedence, paint order, opacity, and even-odd winding.
    @Test public void inheritedPaint() throws Exception {
        var image = SvgImporter.parse(svg("<g fill='red' fill-opacity='.5'><rect width='10' height='10'/>" +
                "<path fill='blue' style='fill:#0f0;fill-rule:evenodd' d='M0 0H20V20H0Z M5 5H15V15H5Z'/></g>"));
        assertEquals(1, image.layers().get(0).paint().stops().get(0).r(), 0);
        assertEquals(.5, image.layers().get(0).paint().stops().get(0).a(), 0);
        assertEquals(1, image.layers().get(1).paint().stops().get(0).g(), 0);
        assertTrue(image.layers().get(1).evenOdd());
    }

    // Gradient IDs resolve regardless of definition order, including inherited stops and transforms.
    @Test public void gradients() throws Exception {
        var image = SvgImporter.parse(svg("<rect width='100' height='50' fill='url(#a)'/>" +
                "<circle cx='20' cy='20' r='10' fill='url(#r)'/>" +
                "<defs><linearGradient id='base'><stop offset='0' stop-color='red'/><stop offset='100%' stop-color='blue' stop-opacity='.25'/></linearGradient>" +
                "<linearGradient id='a' href='#base' gradientTransform='translate(.25 0)' spreadMethod='reflect'/>" +
                "<radialGradient id='r' href='#base' cx='.4' cy='.6' r='.3'/></defs>"));
        var paint = image.layers().get(0).paint();
        assertEquals(1, paint.type());
        assertEquals(2, paint.spread());
        assertEquals(.25, paint.stops().get(1).a(), 0);
        assertEquals(-.25, paint.transform().getTranslateX(), .00001);
        assertEquals(2, image.layers().get(1).paint().type());
    }

    // Malformed paint URLs report an SVG error instead of leaking substring exceptions.
    @Test
    public void malformedPaintReferences() throws Exception {
        invalid("<rect width='10' height='10' fill='url('/>", "Invalid paint reference");
        invalid("<rect width='10' height='10' fill='url(#missing'/>", "Invalid paint reference");
        invalid("<rect width='10' height='10' fill='url(#missing)'/>", "Missing or unsupported paint reference");
    }

    // Empty/degenerate geometry must not fail due to a singular object-bounding-box gradient.
    @Test public void emptyGeometry() throws Exception {
        assertTrue(SvgImporter.parse(svg("<rect width='0' height='5' fill='url(#missing)'/><path d=''/>")).layers().isEmpty());
        var data = SlugCompiler.compileAtlas(new SlugCompiler.Image[]{SvgImporter.toVectorImage(SvgImporter.parse(svg("")))});
        assertEquals(0, data.paints().pixels.getFloat(0), 0);
    }

    // Rejects drawing features that would otherwise silently disappear, including definitions.
    @Test public void unsupportedFeatures() throws Exception {
        for (String tag : new String[]{"text", "image", "use", "clipPath", "mask", "filter", "animateMotion", "style"})
            invalid("<defs><" + tag + "/></defs>", "Unsupported SVG element");
        invalid("<rect width='10' height='10' stroke='red'/>", "Strokes");
        invalid("<g opacity='.5'><rect width='10' height='10'/></g>", "Group opacity");
        invalid("<rect width='10' height='10' style='mix-blend-mode:multiply'/>", "Unsupported style");
        invalid("<path d='M0 0Lbad'/>", "SVG");
    }

    // External document types are accepted without network or filesystem entity expansion.
    @Test public void externalDocumentsDisabled() throws Exception {
        String source = "<!DOCTYPE svg SYSTEM 'file:///does-not-exist.svg'><svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 20 10'><rect width='10' height='10'/></svg>";
        assertEquals(1, SvgImporter.parse(source.getBytes(StandardCharsets.UTF_8)).layers().size());
    }

    // Animation-only references participate in mode inference, including empty-atlas compatibility.
    @Test public void atlasMode() throws Exception {
        assertFalse(AtlasUtil.isVectorAtlas(Atlas.newBuilder().build()));
        var atlas = Atlas.newBuilder().addAnimations(AtlasAnimation.newBuilder().setId("anim").addImages(AtlasImage.newBuilder().setImage("/a.svg")));
        assertTrue(AtlasUtil.isVectorAtlas(atlas.build()));
        atlas.addImages(AtlasImage.newBuilder().setImage("/b.png"));
        try { AtlasUtil.isVectorAtlas(atlas.build()); fail(); }
        catch (CompileExceptionError e) { assertTrue(e.getMessage().contains("cannot mix")); }
    }

    // Compiled SVG metadata preserves variants, pivots, names and flipbooks deterministically.
    @Test public void compiledAtlasMetadata() throws Exception {
        var a = AtlasImage.newBuilder().setImage("/a.svg").setPivotX(.25f).setPivotY(.75f).build();
        var b = AtlasImage.newBuilder().setImage("/b.svg").build();
        var atlas = Atlas.newBuilder().addImages(a).addImages(b).addAnimations(AtlasAnimation.newBuilder()
                .setId("flipbook").setFps(7).setPlayback(Playback.PLAYBACK_LOOP_FORWARD).addImages(b).addImages(a)).build();
        var sources = Map.of("/a.svg", svg("<rect width='50' height='30'/>"), "/b.svg", svg("<circle cx='20' cy='20' r='10'/>"));
        var first = VectorAtlasCompiler.compile(atlas, sources).builder.build();
        var second = VectorAtlasCompiler.compile(atlas, sources).builder.build();
        assertEquals(first, second);
        assertEquals(TextureSetMode.TEXTURE_SET_MODE_VECTOR, first.getMode());
        assertEquals("", first.getTexture());
        assertEquals(2, first.getVector().getImagesCount());
        assertEquals(8, first.getGeometries(0).getVerticesCount());
        assertEquals(6, first.getGeometries(0).getIndicesCount());
        assertEquals(-.25, first.getGeometries(0).getPivotX(), 0);
        assertEquals(-.25, first.getGeometries(0).getPivotY(), 0);
        assertEquals("flipbook", first.getAnimations(0).getId());
        assertEquals(7, first.getAnimations(0).getFps());
        assertEquals(1, first.getFrameIndices(first.getAnimations(0).getStart()));
    }

    // Vector trimming is rejected, while bounded numeric overflow reports a corrective action.
    @Test public void trimAndCapacityErrors() throws Exception {
        var atlas = Atlas.newBuilder().addImages(AtlasImage.newBuilder().setImage("/a.svg").setSpriteTrimMode(SpriteTrimmingMode.SPRITE_TRIM_MODE_4)).build();
        try { VectorAtlasCompiler.compile(atlas, Map.of("/a.svg", svg("<rect width='2' height='2'/>"))); fail(); }
        catch (CompileExceptionError e) { assertTrue(e.getMessage().contains("trimming")); }
        var path = SvgImporter.toVectorImage(SvgImporter.parse(svg("<rect width='2' height='2'/>"))).paths()[0];
        var huge = new SlugCompiler.Path(path.commands(), false, 0, 0, path.parameters(), path.paintTransform(), new float[1025 * 5]);
        try { SlugCompiler.compileAtlas(new SlugCompiler.Image[]{new SlugCompiler.Image(10, 10, new SlugCompiler.Path[]{huge})}); fail(); }
        catch (IllegalArgumentException e) { assertTrue(e.getMessage().contains("capacity exceeded")); }
        StringBuilder outline = new StringBuilder("<path d='M0 0");
        for (int i = 0; i < 1100; ++i) outline.append("L").append(i % 2).append(' ').append(i);
        outline.append("Z'/>");
        var complex = SvgImporter.toVectorImage(SvgImporter.parse(svg(outline.toString())));
        try { SlugCompiler.compileAtlas(new SlugCompiler.Image[]{complex}); fail(); }
        catch (IllegalArgumentException e) { assertTrue(e.getMessage().contains("capacity exceeded")); }
    }
    // Default offsets and inherited stop colors must follow SVG rather than font palette rules.
    @Test
    public void defaultGradientStopsAndNamedColors() throws Exception {
        var image = SvgImporter.parse(svg("<defs color='aliceblue'><linearGradient id='g'><stop stop-color='currentColor'/><stop offset='1' stop-color='darkslategray'/></linearGradient></defs><rect width='100' height='50' fill='url(#g)'/>"));
        assertEquals(0, image.layers().get(0).paint().stops().get(0).offset(), 0);
        assertEquals(240 / 255f, image.layers().get(0).paint().stops().get(0).r(), 0);
        assertEquals(47 / 255f, image.layers().get(0).paint().stops().get(1).r(), 0);
        var empty = SvgImporter.parse(svg("<defs><linearGradient id='g'/></defs><rect width='100' height='50' fill='url(#g)'/>"));
        assertEquals(0, empty.layers().get(0).paint().stops().get(0).a(), 0);
        SlugCompiler.compileAtlas(new SlugCompiler.Image[]{SvgImporter.toVectorImage(empty)});
    }

}
