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
import com.dynamo.gamesys.proto.AtlasProto.Atlas;
import com.dynamo.gamesys.proto.AtlasProto.AtlasImage;
import com.dynamo.gamesys.proto.AtlasProto.AtlasAnimation;
import com.dynamo.gamesys.proto.Tile.Playback;
import java.io.IOException;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.Map;
import org.junit.Test;

public class SvgAnimationTest {
    private static byte[] svg(String body) {
        return ("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 50'>" + body + "</svg>").getBytes(StandardCharsets.UTF_8);
    }

    // The independent source sampler retains transform order, additive bases and frozen endpoints.
    @Test public void transforms() throws Exception {
        byte[] source = svg("<g transform='translate(4 5)'><animateTransform attributeName='transform' type='translate' from='0 0' to='20 10' dur='1s' fill='freeze' additive='sum'/><rect width='10' height='10'/></g>");
        var half = SvgImporter.parseAtTime(source, .5).layers().get(0).path().getBounds2D();
        assertEquals(14, half.getX(), .00001); assertEquals(10, half.getY(), .00001);
        assertEquals(24, SvgImporter.parseAtTime(source, 1).layers().get(0).path().getBounds2D().getX(), .00001);
    }

    // Presentation tracks override inline styles and preserve straight-sRGB color and opacity interpolation.
    @Test public void paintAndOpacity() throws Exception {
        byte[] source = svg("<rect width='20' height='20' style='fill:green'><animate attributeName='fill' from='red' to='blue' dur='1s' fill='freeze'/><animate attributeName='fill-opacity' from='1' to='0' dur='1s' fill='freeze'/></rect>");
        var color = SvgImporter.parseAtTime(source, .5).layers().get(0).paint().stops().get(0);
        assertEquals(.5, color.r(), .00001); assertEquals(0, color.g(), 0); assertEquals(.5, color.b(), .00001); assertEquals(.5, color.a(), .00001);
    }

    // Gradient transforms and stop tracks preserve their independently sampled reference values.
    @Test public void gradients() throws Exception {
        byte[] source = svg("<defs><linearGradient id='g'><animateTransform attributeName='gradientTransform' type='translate' from='0 0' to='1 0' dur='1s' fill='freeze'/><stop stop-color='red'><animate attributeName='stop-color' from='red' to='blue' dur='1s' fill='freeze'/></stop><stop offset='1' stop-color='white'/></linearGradient></defs><rect width='100' height='50' fill='url(#g)'/>");
        var paint = SvgImporter.parseAtTime(source, .5).layers().get(0).paint();
        assertEquals(-.5, paint.transform().getTranslateX(), .00001); assertEquals(.5, paint.stops().get(0).r(), .00001); assertEquals(.5, paint.stops().get(0).b(), .00001);
    }

    // Timed visibility reverts at its endpoint while compact compilation retains the hidden geometry once.
    @Test public void visibilitySet() throws Exception {
        byte[] source = svg("<rect width='20' height='20' visibility='hidden'><set attributeName='visibility' to='visible' begin='.25s' dur='.5s'/></rect>");
        assertTrue(SvgImporter.parseAtTime(source, 0).layers().isEmpty());
        assertEquals(1, SvgImporter.parseAtTime(source, .25).layers().size());
        assertTrue(SvgImporter.parseAtTime(source, .75).layers().isEmpty());
        assertEquals(1, SvgImporter.parseVector(source).image().layers().size());
    }

    // Loop periods and authored spline/discrete keys survive without a sampled-frame array.
    @Test public void loopingAndEasing() throws Exception {
        byte[] source = svg("<rect width='20' height='20'><animate attributeName='fill-opacity' values='0;1' calcMode='spline' keyTimes='0;1' keySplines='.42 0 1 1' dur='1s' repeatCount='indefinite'/><animate attributeName='fill' values='red;blue' calcMode='discrete' dur='2s' repeatCount='indefinite'/></rect>");
        var parsed = SvgImporter.parseVector(source);
        assertTrue(parsed.animation().loop()); assertEquals(2, parsed.animation().duration(), 0);
        assertEquals(2, parsed.animation().tracks.size());
        var color = SvgImporter.parseAtTime(source, .5).layers().get(0).paint().stops().get(0);
        assertTrue(color.a() > .25 && color.a() < .4);
        assertEquals(1, SvgImporter.parseAtTime(source, 1.5).layers().get(0).paint().stops().get(0).b(), 0);
    }

    // Unsupported geometry/timing, invalid keyframes and isolated group opacity fail before native compilation.
    @Test public void invalidTracks() throws Exception {
        for (String track : new String[]{
                "<animate attributeName='d' from='M0 0' to='M1 1' dur='1s'/>",
                "<animate attributeName='fill' from='red' to='blue' begin='other.end' dur='1s'/>",
                "<animate attributeName='fill-opacity' values='0;1' keyTimes='1;0' dur='1s'/>",
                "<animate attributeName='fill-opacity' from='0' to='1' dur='0s'/>",
                "<animate attributeName='fill-opacity' from='0' to='1' dur='1e-300s'/>",
                "<animate attributeName='fill-opacity' from='0' to='1' dur='1e100s'/>",
                "<animate attributeName='fill-opacity' from='0' to='1' dur='1s' repeatCount='indefinite' begin='.5s'/>"}) {
            try { SvgImporter.parseVector(svg("<rect width='10' height='10'>" + track + "</rect>")); fail(track); }
            catch (IOException expected) { assertFalse(expected.getMessage().isEmpty()); }
        }
        try { SvgImporter.parseVector(svg("<rect width='10' height='10'><animate attributeName='fill-opacity' values='" + "1;".repeat(256) + "1' dur='1s'/></rect>")); fail(); }
        catch (IOException expected) { assertTrue(expected.getMessage().contains("256 keys")); }
        try { SvgImporter.parseVector(svg("<g><animate attributeName='opacity' from='0' to='1' dur='1s'/><rect width='10' height='10'/></g>")); fail(); }
        catch (IOException expected) { assertTrue(expected.getMessage().contains("Group opacity")); }
    }

    // The compiled atlas keeps one image with immutable keys, pivots, clip timing and deterministic editor metadata.
    @Test public void compiledVectorTracks() throws Exception {
        var image = AtlasImage.newBuilder().setImage("/pulse.svg").setPivotX(.25f).build();
        var atlas = Atlas.newBuilder().addImages(image).addAnimations(AtlasAnimation.newBuilder().setId("loop").setFps(60).setPlayback(Playback.PLAYBACK_LOOP_FORWARD).addImages(image)).build();
        var sources = Map.of("/pulse.svg", svg("<rect width='20' height='20'><animate attributeName='fill' from='red' to='blue' dur='1s' fill='freeze'/></rect>"));
        var compiled = VectorAtlasCompiler.compile(atlas, sources);
        var textureSet = compiled.builder.build();
        assertEquals(1, textureSet.getGeometriesCount()); assertEquals(1, textureSet.getVector().getImagesCount());
        assertEquals(1, textureSet.getAnimations(0).getEnd() - textureSet.getAnimations(0).getStart());
        assertEquals(1, textureSet.getAnimations(0).getDuration(), 0);
        assertEquals(Playback.PLAYBACK_LOOP_FORWARD, textureSet.getAnimations(0).getPlayback());
        var implicit = textureSet.getAnimations(1);
        assertEquals("pulse", implicit.getId()); assertEquals(Playback.PLAYBACK_ONCE_FORWARD, implicit.getPlayback());
        assertEquals(-.25, textureSet.getGeometries(0).getPivotX(), 0);
        var paints = textureSet.getVector().getPaints().asReadOnlyByteBuffer().order(ByteOrder.LITTLE_ENDIAN);
        assertTrue(paints.getFloat(textureSet.getVector().getImages(0).getPaint() * 16 + 8) > 0);
        assertEquals(textureSet, VectorAtlasCompiler.compile(atlas, sources).builder.build());
        assertEquals(textureSet, VectorAtlasCompiler.createTextureSet(atlas, compiled.layoutResult.layouts, textureSet.getVector()).builder.build());
        var renamed = atlas.toBuilder().setRenamePatterns("pulse=renamed").build();
        assertEquals(VectorAtlasCompiler.compile(renamed, sources).builder.build(), VectorAtlasCompiler.createTextureSet(renamed, compiled.layoutResult.layouts, textureSet.getVector()).builder.build());
    }

    // Extending an authored clip changes its duration, not the geometry or texture allocation.
    @Test public void longClipDoesNotDuplicateGeometry() throws Exception {
        var atlas = Atlas.newBuilder().addImages(AtlasImage.newBuilder().setImage("/pulse.svg")).build();
        String body = "<rect width='20' height='20'><animate attributeName='fill' from='red' to='blue' dur='%ss' fill='freeze'/></rect>";
        var shortClip = VectorAtlasCompiler.compile(atlas, Map.of("/pulse.svg", svg(String.format(body, "1")))).builder.build();
        var longClip = VectorAtlasCompiler.compile(atlas, Map.of("/pulse.svg", svg(String.format(body, "600")))).builder.build();
        assertEquals(shortClip.getVector().getCurves(), longClip.getVector().getCurves());
        assertEquals(shortClip.getVector().getBands(), longClip.getVector().getBands());
        assertEquals(shortClip.getVector().getPaints().size(), longClip.getVector().getPaints().size());
        assertEquals(1, longClip.getGeometriesCount()); assertEquals(600, longClip.getAnimations(0).getDuration(), 0);
    }
    // Continuous tracks accept periods below the former 60fps sampling grid.
    @Test public void fractionalLoopPeriods() throws Exception {
        var parsed = SvgImporter.parseVector(svg("<rect width='20' height='20'><animate attributeName='fill-opacity' values='0;1' dur='.013s' repeatCount='indefinite'/><animate attributeName='fill' values='red;blue' dur='.026s' repeatCount='indefinite'/></rect>"));
        assertEquals(.026, parsed.animation().duration(), .000000001);
    }

    // Canonical gradients retain all stops when their initial coordinates/radius are degenerate.
    @Test public void animatedDegenerateGradientRetainsStops() throws Exception {
        var atlas = Atlas.newBuilder().addImages(AtlasImage.newBuilder().setImage("/gradient.svg")).build();
        var compiled = VectorAtlasCompiler.compile(atlas, Map.of("/gradient.svg", svg("<defs><linearGradient id='g' x2='0'><animate attributeName='x2' from='0' to='1' dur='1s' fill='freeze'/><stop stop-color='red'/><stop offset='1' stop-color='blue'/></linearGradient></defs><rect width='20' height='20' fill='url(#g)'/>"))).builder.build();
        var paints = compiled.getVector().getPaints().asReadOnlyByteBuffer().order(ByteOrder.LITTLE_ENDIAN);
        int layer = compiled.getVector().getImages(0).getPaint() + 1;
        int paint = (int)paints.getFloat((layer + 1) * 16 + 12);
        assertEquals(1, paints.getFloat(paint * 16), 0);
        assertEquals(2, paints.getFloat(paint * 16 + 4), 0);
    }

    // Canonical paths are normalized before float conversion, retaining small paths at large source offsets.
    @Test public void normalizedGeometryKeepsLayerBindings() throws Exception {
        var atlas = Atlas.newBuilder().addImages(AtlasImage.newBuilder().setImage("/offset.svg")).build();
        var compiled = VectorAtlasCompiler.compile(atlas, Map.of("/offset.svg", svg("<g transform='translate(-1000000000 0)'><rect x='1000000000' width='1' height='10' fill='red'/></g><rect x='10' width='20' height='20'><animate attributeName='fill' from='red' to='blue' dur='1s'/></rect>"))).builder.build();
        var paints = compiled.getVector().getPaints().asReadOnlyByteBuffer().order(ByteOrder.LITTLE_ENDIAN);
        assertEquals(2, paints.getFloat(compiled.getVector().getImages(0).getPaint() * 16), 0);
    }

}
