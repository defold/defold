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

import java.awt.geom.AffineTransform;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import org.w3c.dom.Element;
import org.w3c.dom.Node;

/**
 * Immutable keyframes and layer bindings appended to the atlas paint texture.
 * Image header Z points to nine texels per layer; tracks and keys use three
 * texels each. The matching decoder is builtins/materials/slug-animation.glsl.
 */
final class SvgAnimationData {
    private final List<Float> data = new ArrayList<>();
    private final Map<Object, Integer> records = new HashMap<>();
    private final Map<String, Element> ids = new HashMap<>();
    private SvgAnimation animation;
    private record Binding(List<Element> elements, String name, String fallback, double units, boolean inherited) {}
    private record EncodedTrack(SvgAnimation.Track track, double units) {}
    private record Transform(AffineTransform base, List<SvgAnimation.Track> tracks) {}

    SvgAnimationData(ByteBuffer paints, int usedTexels) {
        ByteBuffer input = paints.duplicate().order(ByteOrder.LITTLE_ENDIAN);
        for (int i = 0; i < usedTexels * 4; ++i) data.add(input.getFloat(i * 4));
    }

    private int reserve(int count) throws IOException {
        int at = data.size() / 4;
        if (at + count > 4096 * 256) throw new IOException("Vector atlas animation capacity exceeded (4096 x 256 paint texels). Split the atlas.");
        for (int i = 0; i < count * 4; ++i) data.add(0f);
        return at;
    }

    private void put(int at, double... values) throws IOException {
        for (int i = 0; i < values.length; ++i) {
            float value = (float)values[i];
            if (!Float.isFinite(value)) throw new IOException("SVG animation value exceeds the supported numeric range");
            data.set(at * 4 + i, value);
        }
    }

    private int vector(double... values) throws IOException {
        int at = reserve(1); put(at, values); return at;
    }

    private void matrix(int at, AffineTransform transform) throws IOException {
        double[] m = new double[6]; transform.getMatrix(m);
        put(at, m[0], m[2], m[4], 0, m[1], m[3], m[5], 0);
    }

    private int pointers(List<Integer> values) throws IOException {
        int at = reserve((values.size() + 3) / 4);
        for (int i = 0; i < values.size(); ++i) data.set(at * 4 + i, (float)(int)values.get(i));
        return at;
    }

    private static List<Element> ancestry(Element e) {
        List<Element> elements = new ArrayList<>();
        for (Node n = e; n instanceof Element element; n = n.getParentNode()) elements.add(0, element);
        return elements;
    }

    private static String attribute(Element e, String name) {
        String value = e.hasAttribute(name) ? e.getAttribute(name).trim() : null;
        for (String declaration : e.getAttribute("style").split(";")) {
            int colon = declaration.indexOf(':');
            if (colon >= 0 && declaration.substring(0, colon).trim().equals(name)) value = declaration.substring(colon + 1).trim();
        }
        return value == null || value.equals("inherit") ? null : value;
    }

    private List<SvgAnimation.Track> tracks(Element element, String name) {
        return animation.tracks.stream().filter(t -> t.target() == element && t.name().equals(name)).toList();
    }

    private static double[] value(String name, String text, double units) throws IOException {
        if (name.equals("fill") || name.equals("color") || name.equals("stop-color")) {
            if (text.startsWith("url(")) return new double[]{0, 0, 0, -1};
            float[] color = text.equals("none") ? new float[4] : SvgImporter.color(text);
            return new double[]{color[0], color[1], color[2], color[3]};
        }
        if (name.equals("visibility")) return new double[]{text.equals("visible") ? 1 : 0};
        if (name.equals("display")) return new double[]{text.equals("none") ? 0 : 1};
        return new double[]{SvgImporter.length(text, units)};
    }

    private int track(SvgAnimation.Track track, double units) throws IOException {
        EncodedTrack key = new EncodedTrack(track, units);
        Integer cached = records.get(key); if (cached != null) return cached;
        int at = reserve(3);
        records.put(key, at);
        int keys = reserve(track.values().size() * 3);
        int type = switch (track.type()) { case "translate" -> 1; case "scale" -> 2; case "rotate" -> 3; case "skewX" -> 4; case "skewY" -> 5; default -> 0; };
        int mode = track.mode().equals("discrete") ? 1 : track.mode().equals("spline") ? 2 : 0;
        put(at, track.values().size(), mode, type, track.additive() ? 1 : 0);
        put(at + 1, track.begin(), track.duration(), Double.isInfinite(track.repeats()) ? -1 : track.repeats(), track.freeze() ? 1 : 0);
        put(at + 2, keys, 0, 0, 0);
        for (int i = 0; i < track.values().size(); ++i) {
            double[] v = type == 0 ? value(track.name(), track.values().get(i), units) : SvgImporter.numbers(track.values().get(i));
            if (type == 2 && v.length == 1) v = new double[]{v[0], v[0]};
            put(keys + i * 3, v);
            double[] ease = mode == 2 && i < track.splines().length ? track.splines()[i] : new double[]{0, 0, 1, 1};
            put(keys + i * 3 + 1, track.times()[i], ease[0], ease[1], ease[2]);
            put(keys + i * 3 + 2, ease[3], 0, 0, 0);
        }
        return at;
    }

    private int binding(List<Element> elements, String name, String fallback, double units, boolean inherited) throws IOException {
        Binding key = new Binding(List.copyOf(elements), name, fallback, units, inherited);
        Integer cached = records.get(key); if (cached != null) return cached;
        String base = fallback;
        List<SvgAnimation.Track> active = new ArrayList<>();
        for (Element element : elements) {
            if (!inherited) { base = fallback; active.clear(); }
            String own = attribute(element, name);
            if (own != null) { base = own; active.clear(); }
            active.addAll(tracks(element, name));
        }
        if (base.equals("currentColor")) {
            List<SvgAnimation.Track> propertyTracks = active;
            active = new ArrayList<>();
            base = "black";
            for (Element element : elements) {
                String own = attribute(element, "color");
                if (own != null) { base = own; active.clear(); }
                active.addAll(tracks(element, "color"));
            }
            active.addAll(propertyTracks);
        }
        int at = reserve(2); records.put(key, at);
        put(at, value(name, base, units));
        List<Integer> indices = new ArrayList<>();
        for (SvgAnimation.Track t : active) indices.add(track(t, units));
        put(at + 1, indices.size(), pointers(indices), 0, 0);
        return at;
    }

    private int binding(Element element, String name, String fallback, double units, boolean inherited) throws IOException {
        return binding(ancestry(element), name, fallback, units, inherited);
    }

    private int transform(AffineTransform base, List<SvgAnimation.Track> tracks) throws IOException {
        Transform key = new Transform(base, List.copyOf(tracks));
        Integer cached = records.get(key); if (cached != null) return cached;
        int at = reserve(3); records.put(key, at); matrix(at, base);
        List<Integer> indices = new ArrayList<>();
        for (SvgAnimation.Track t : tracks) indices.add(track(t, 1));
        put(at + 2, indices.size(), pointers(indices), 0, 0);
        return at;
    }

    private static void collect(Element e, Map<String, Element> ids) {
        if (e.hasAttribute("id")) ids.put(e.getAttribute("id"), e);
        for (Node n = e.getFirstChild(); n != null; n = n.getNextSibling()) if (n instanceof Element child) collect(child, ids);
    }

    private List<Element> gradientAncestry(Element gradient) {
        List<Element> result = new ArrayList<>();
        for (Element e = gradient; e != null;) {
            result.add(0, e);
            String href = e.hasAttribute("href") ? e.getAttribute("href") : e.getAttributeNS("http://www.w3.org/1999/xlink", "href");
            e = href.startsWith("#") ? ids.get(href.substring(1)) : null;
        }
        return result;
    }

    void append(SvgAnimation animation, int paint) throws IOException {
        if (!animation.animated()) return;
        if ((int)(float)data.get(paint * 4) != animation.layers.size())
            throw new IOException("SVG animation geometry contains a path that collapses at compiler precision. Simplify the path or bake its transforms.");
        this.animation = animation;
        records.clear(); ids.clear(); collect(animation.root, ids);
        int extensions = reserve(animation.layers.size() * 9);
        data.set(paint * 4 + 2, (float)extensions);
        for (int i = 0; i < animation.layers.size(); ++i) {
            SvgAnimation.Layer layer = animation.layers.get(i);
            Element element = layer.element();
            List<Element> ancestors = ancestry(element);
            List<Integer> transforms = new ArrayList<>(), displays = new ArrayList<>();
            for (Element ancestor : ancestors) {
                AffineTransform base = SvgImporter.transform(ancestor.getAttribute("transform"));
                List<SvgAnimation.Track> tracks = tracks(ancestor, "transform");
                if (!base.isIdentity() || !tracks.isEmpty()) transforms.add(transform(base, tracks));
                if (attribute(ancestor, "display") != null || !tracks(ancestor, "display").isEmpty())
                    displays.add(binding(List.of(ancestor), "display", "inline", 1, false));
            }
            int at = extensions + i * 9;
            put(at, pointers(transforms), transforms.size(), binding(element, "fill", "black", 1, true), binding(element, "fill-opacity", "1", 1, true));
            put(at + 1, binding(element, "opacity", "1", 1, false), binding(element, "visibility", "visible", 1, true), pointers(displays), displays.size());
            matrix(at + 3, animation.viewport);
            matrix(at + 7, layer.geometryTransform());
            matrix(at + 5, new AffineTransform());
            String fill = SvgImporter.inheritedStyle(element).getOrDefault("fill", "black");
            if (!fill.startsWith("url(")) continue;
            String id = fill.substring(4, fill.length() - 1).trim().replace("\"", "").replace("'", "");
            Element gradient = ids.get(id.substring(1));
            List<Element> stops = new ArrayList<>();
            Map<String, String> g = SvgImporter.gradient(gradient, ids, new HashSet<>(), stops);
            List<Element> gradientChain = gradientAncestry(gradient);
            List<SvgAnimation.Track> gradientTracks = new ArrayList<>();
            for (Element target : gradientChain) {
                if (target.hasAttribute("gradientTransform")) gradientTracks.clear();
                gradientTracks.addAll(tracks(target, "gradientTransform"));
            }
            AffineTransform baseGradient = SvgImporter.transform(g.getOrDefault("gradientTransform", ""));
            matrix(at + 5, baseGradient);
            int gradientTransform = transform(baseGradient, gradientTracks);
            int stopData = reserve(stops.size());
            for (int j = 0; j < stops.size(); ++j) {
                Element stop = stops.get(j);
                put(stopData + j, binding(stop, "stop-color", "black", 1, true), binding(stop, "stop-opacity", "1", 1, true), binding(List.of(stop), "offset", "0", 1, false), 0);
            }
            boolean object = g.getOrDefault("gradientUnits", "objectBoundingBox").equals("objectBoundingBox");
            double width = object ? 1 : layer.width(), height = object ? 1 : layer.height();
            int coordinates = reserve(2);
            if (gradient.getLocalName().equals("linearGradient")) {
                put(coordinates, binding(gradientChain, "x1", "0%", width, true), binding(gradientChain, "y1", "0%", height, true),
                        binding(gradientChain, "x2", "100%", width, true), binding(gradientChain, "y2", "0%", height, true));
            } else {
                int cx = binding(gradientChain, "cx", "50%", width, true), cy = binding(gradientChain, "cy", "50%", height, true);
                boolean hasFx = gradientChain.stream().anyMatch(e -> e.hasAttribute("fx") || !tracks(e, "fx").isEmpty());
                boolean hasFy = gradientChain.stream().anyMatch(e -> e.hasAttribute("fy") || !tracks(e, "fy").isEmpty());
                put(coordinates, hasFx ? binding(gradientChain, "fx", "50%", width, true) : cx,
                        hasFy ? binding(gradientChain, "fy", "50%", height, true) : cy, 0, cx);
                put(coordinates + 1, cy, binding(gradientChain, "r", "50%", Math.hypot(width, height) / Math.sqrt(2), true), 0, 0);
            }
            put(at + 2, gradientTransform, stopData, stops.size(), coordinates);
        }
    }

    ByteBuffer finish() {
        int texels = data.size() / 4;
        int size = Math.max(4096, (texels + 4095) & ~4095) * 16;
        ByteBuffer output = ByteBuffer.allocate(size).order(ByteOrder.LITTLE_ENDIAN);
        for (float value : data) output.putFloat(value);
        output.position(0);
        return output;
    }
}
