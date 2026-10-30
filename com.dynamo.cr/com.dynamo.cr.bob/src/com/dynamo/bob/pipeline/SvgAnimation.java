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

import java.io.IOException;
import java.math.BigDecimal;
import java.math.BigInteger;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import org.w3c.dom.Element;
import org.w3c.dom.Node;

/** Parsed SVG keyframe tracks. Sampling is used by the initial SVG source preview. */
final class SvgAnimation {
    private static final Set<String> COLORS = Set.of("fill", "color", "stop-color");
    private static final Set<String> NUMBERS = Set.of("fill-opacity", "opacity", "stop-opacity", "offset",
            "x1", "y1", "x2", "y2", "cx", "cy", "r", "fx", "fy");
    private static final Set<String> DISCRETE = Set.of("visibility", "display");
    private record Property(Element target, String name) {}
    private record Base(boolean present, String value) {}
    record Track(Element target, String name, String type, List<String> values, double[] times,
                         double[][] splines, String mode, double begin, double duration, double repeats,
                         boolean freeze, boolean additive, boolean set) {}

    final Element root;
    final List<Track> tracks = new ArrayList<>();
    private final Map<Property, Base> bases = new LinkedHashMap<>();
    private final Map<Element, String> styles = new LinkedHashMap<>();
    private final double duration;
    java.awt.geom.AffineTransform viewport;
    final List<Layer> layers = new ArrayList<>();
    record Layer(Element element, java.awt.geom.AffineTransform geometryTransform, double width, double height) {}
    private final boolean loop;

    SvgAnimation(Element root) throws IOException {
        this.root = root;
        Map<String, Element> ids = new HashMap<>();
        List<Element> animations = new ArrayList<>();
        collect(root, ids, animations);
        for (Element animation : animations) {
            Track track = read(animation, ids);
            tracks.add(track);
            Property property = new Property(track.target, track.name);
            bases.putIfAbsent(property, new Base(track.target.hasAttribute(track.name), track.target.getAttribute(track.name)));
            styles.putIfAbsent(track.target, track.target.getAttribute("style"));
            animation.getParentNode().removeChild(animation);
        }
        tracks.sort(Comparator.comparingDouble(Track::begin));
        loop = tracks.stream().anyMatch(t -> Double.isInfinite(t.repeats));
        double clipDuration = 0;
        BigDecimal period = BigDecimal.ZERO;
        for (Track track : tracks) {
            if (loop) {
                if (track.begin != 0 || !Double.isInfinite(track.repeats))
                    throw error("Looping tracks must all begin at 0 and use repeatCount=indefinite. Use a finite SVG clip and an atlas loop for introductions or timed set elements.");
                BigDecimal seconds = BigDecimal.valueOf(track.duration).stripTrailingZeros();
                if (period.signum() == 0) period = seconds;
                else {
                    int scale = Math.max(period.scale(), seconds.scale());
                    BigInteger a = period.movePointRight(scale).toBigIntegerExact();
                    BigInteger b = seconds.movePointRight(scale).toBigIntegerExact();
                    period = new BigDecimal(a.divide(a.gcd(b)).multiply(b), scale);
                }
                clipDuration = period.doubleValue();
                if (!Float.isFinite((float)clipDuration)) throw error("Common loop duration exceeds the supported numeric range");
            } else {
                clipDuration = Math.max(clipDuration, track.begin + track.duration * track.repeats);
            }
        }
        if (tracks.size() > 256) throw error("Animation capacity exceeded: at most 256 tracks per SVG");
        if (!Float.isFinite((float)clipDuration)) throw error("Clip duration exceeds the supported numeric range");
        duration = clipDuration;
    }

    double duration() { return duration; }
    boolean animated() { return !tracks.isEmpty(); }
    boolean loop() { return loop; }

    Element sample(double time) throws IOException {
        for (var entry : bases.entrySet()) {
            Property p = entry.getKey();
            Base base = entry.getValue();
            if (base.present) p.target.setAttribute(p.name, base.value); else p.target.removeAttribute(p.name);
        }
        for (var entry : styles.entrySet()) entry.getKey().setAttribute("style", entry.getValue());
        for (Track track : tracks) {
            if (time < track.begin) continue;
            double elapsed = time - track.begin;
            double end = track.duration * track.repeats;
            if (elapsed >= end && !track.freeze) continue;
            double fraction = elapsed >= end ? 1 : (elapsed % track.duration) / track.duration;
            String value = value(track, fraction);
            if (track.additive) value = track.target.getAttribute(track.name) + " " + value;
            track.target.setAttribute(track.name, value);
            // SMIL overrides a property's inline style as well as presentation attributes.
            if (COLORS.contains(track.name) || Set.of("fill-opacity", "opacity", "stop-opacity").contains(track.name) || DISCRETE.contains(track.name)) {
                StringBuilder style = new StringBuilder();
                for (String declaration : track.target.getAttribute("style").split(";")) {
                    int colon = declaration.indexOf(':');
                    if (colon < 0 || !declaration.substring(0, colon).trim().equals(track.name))
                        style.append(declaration).append(';');
                }
                track.target.setAttribute("style", style.append(track.name).append(':').append(value).toString());
            }
        }
        return root;
    }

    private static IOException error(String message) { return new IOException("SVG animation: " + message); }

    private static void collect(Element element, Map<String, Element> ids, List<Element> animations) throws IOException {
        if (element.hasAttribute("id") && ids.put(element.getAttribute("id"), element) != null)
            throw error("Duplicate id: " + element.getAttribute("id"));
        if (Set.of("animate", "animateTransform", "set").contains(element.getLocalName())) animations.add(element);
        for (Node child = element.getFirstChild(); child != null; child = child.getNextSibling())
            if (child instanceof Element e) collect(e, ids, animations);
    }

    private static double clock(String text) throws IOException {
        text = text.trim();
        double factor = text.endsWith("ms") ? .001 : 1;
        if (text.endsWith("ms")) text = text.substring(0, text.length() - 2);
        else if (text.endsWith("s")) text = text.substring(0, text.length() - 1);
        return SvgImporter.number(text) * factor;
    }

    private static Track read(Element animation, Map<String, Element> ids) throws IOException {
        String href = animation.hasAttribute("href") ? animation.getAttribute("href") : animation.getAttributeNS("http://www.w3.org/1999/xlink", "href");
        Element target = href.isEmpty() && animation.getParentNode() instanceof Element parent ? parent :
                href.startsWith("#") ? ids.get(href.substring(1)) : null;
        if (target == null) throw error("Animation targets must be elements in the same SVG");
        for (String unsupported : List.of("end", "repeatDur", "min", "max", "by"))
            if (animation.hasAttribute(unsupported)) throw error("Unsupported timing/value attribute: " + unsupported);
        if (!Set.of("", "none").contains(animation.getAttribute("accumulate"))) throw error("accumulate is unsupported");
        String name = animation.getAttribute("attributeName");
        String type = animation.getLocalName().equals("animateTransform") ? animation.getAttribute("type") : "";
        boolean set = animation.getLocalName().equals("set");
        if (!type.isEmpty()) {
            if (!Set.of("transform", "gradientTransform").contains(name) || !Set.of("translate", "scale", "rotate", "skewX", "skewY").contains(type))
                throw error("animateTransform requires transform/gradientTransform and translate, scale, rotate, skewX or skewY");
        } else if (!COLORS.contains(name) && !NUMBERS.contains(name) && !DISCRETE.contains(name)) {
            throw error("Unsupported animated attribute: " + name);
        }
        String tag = target.getLocalName();
        boolean drawing = Set.of("svg", "g", "path", "rect", "circle", "ellipse", "polygon", "polyline", "line").contains(tag);
        boolean gradient = Set.of("linearGradient", "radialGradient").contains(tag);
        boolean supportedTarget = switch (name) {
            case "transform", "fill", "fill-opacity", "opacity", "visibility", "display" -> drawing;
            case "gradientTransform" -> gradient;
            case "stop-color", "stop-opacity", "offset" -> tag.equals("stop");
            case "x1", "y1", "x2", "y2" -> tag.equals("linearGradient");
            case "cx", "cy", "r", "fx", "fy" -> tag.equals("radialGradient");
            case "color" -> drawing || gradient || tag.equals("stop");
            default -> false;
        };
        if (!supportedTarget) throw error("Unsupported " + name + " animation target: " + tag);
        if (name.equals("opacity") && Set.of("g", "svg").contains(target.getLocalName()))
            throw error("Group opacity is unsupported; animate fill-opacity instead");
        String mode = set ? "discrete" : animation.getAttribute("calcMode");
        if (mode.isEmpty()) mode = "linear";
        if (!Set.of("linear", "discrete", "spline").contains(mode)) throw error("Supported calcMode values are linear, discrete and spline");
        if (DISCRETE.contains(name) && !mode.equals("discrete")) throw error(name + " requires calcMode=discrete or set");
        List<String> values = new ArrayList<>();
        if (set) values.add(animation.getAttribute("to"));
        else if (animation.hasAttribute("values")) for (String value : animation.getAttribute("values").split(";", -1)) values.add(value.trim());
        else if (animation.hasAttribute("from") && animation.hasAttribute("to")) {
            values.add(animation.getAttribute("from")); values.add(animation.getAttribute("to"));
        } else throw error("Specify values, or both from and to");
        if (values.stream().anyMatch(String::isBlank) || (!set && values.size() < 2)) throw error("Animation values must not be empty");
        if (values.size() > 256) throw error("Animation capacity exceeded: at most 256 keys per track");
        for (String value : values) validateValue(name, type, value);
        double[] times = new double[values.size()];
        if (animation.hasAttribute("keyTimes")) {
            String[] entries = animation.getAttribute("keyTimes").split(";", -1);
            if (entries.length != times.length) throw error("keyTimes must match values");
            for (int i = 0; i < times.length; ++i) times[i] = SvgImporter.number(entries[i].trim());
        } else {
            for (int i = 0; i < times.length; ++i) times[i] = i / (double)(mode.equals("discrete") ? times.length : times.length - 1);
        }
        for (int i = 0; i < times.length; ++i)
            if (times[i] < 0 || times[i] > 1 || (i > 0 && times[i] <= times[i - 1])) throw error("keyTimes must increase from 0 to 1");
        if (times[0] != 0 || (!mode.equals("discrete") && times[times.length - 1] != 1)) throw error("keyTimes must start at 0 and end at 1");
        double[][] splines = new double[0][];
        if (mode.equals("spline")) {
            String[] entries = animation.getAttribute("keySplines").split(";", -1);
            if (entries.length != values.size() - 1) throw error("keySplines requires one cubic easing curve per interval");
            splines = new double[entries.length][];
            for (int i = 0; i < entries.length; ++i) {
                splines[i] = SvgImporter.numbers(entries[i]);
                if (splines[i].length != 4) throw error("A keySpline requires four numbers");
                for (double value : splines[i]) if (value < 0 || value > 1) throw error("keySpline values must be between 0 and 1");
            }
        }
        double begin = animation.hasAttribute("begin") ? clock(animation.getAttribute("begin")) : 0;
        if (!animation.hasAttribute("dur")) throw error("Specify a finite positive dur, for example dur=1s");
        double duration = clock(animation.getAttribute("dur"));
        if (duration < Float.MIN_NORMAL || !Float.isFinite((float)duration)) throw error("dur must fit the positive 32-bit animation time range");
        double repeats = animation.getAttribute("repeatCount").equals("indefinite") ? Double.POSITIVE_INFINITY :
                animation.hasAttribute("repeatCount") ? SvgImporter.number(animation.getAttribute("repeatCount")) : 1;
        if (begin < 0 || duration <= 0 || repeats <= 0 || (!Double.isInfinite(repeats) && repeats != Math.rint(repeats)))
            throw error("Use a nonnegative begin, a positive dur, and a positive integer repeatCount or indefinite");
        String fill = animation.getAttribute("fill");
        if (!Set.of("", "remove", "freeze").contains(fill)) throw error("Animation fill must be remove or freeze");
        String additive = animation.getAttribute("additive");
        if (!Set.of("", "replace", "sum").contains(additive) || (additive.equals("sum") && type.isEmpty()))
            throw error("additive=sum is only supported for transforms");
        return new Track(target, name, type, List.copyOf(values), times, splines, mode, begin, duration, repeats, fill.equals("freeze"), additive.equals("sum"), set);
    }

    private static void validateValue(String name, String type, String value) throws IOException {
        if (!type.isEmpty()) {
            double[] v = SvgImporter.numbers(value);
            if ((type.equals("rotate") && v.length != 1 && v.length != 3) ||
                    ((type.equals("translate") || type.equals("scale")) && v.length != 1 && v.length != 2) ||
                    ((type.equals("skewX") || type.equals("skewY")) && v.length != 1)) throw error("Invalid " + type + " value: " + value);
            SvgImporter.transform(type + "(" + value + ")");
        } else if (COLORS.contains(name)) SvgImporter.color(value);
        else if (DISCRETE.contains(name)) {
            Set<String> allowed = name.equals("visibility") ? Set.of("visible", "hidden") : Set.of("inline", "none");
            if (!allowed.contains(value)) throw error("Unsupported " + name + " value: " + value);
        } else SvgImporter.number(value.endsWith("%") ? value.substring(0, value.length() - 1) : value);
    }

    private static String value(Track track, double fraction) throws IOException {
        int lo = 0;
        while (lo + 1 < track.times.length && fraction >= track.times[lo + 1]) ++lo;
        if (track.set || track.mode.equals("discrete") || lo == track.values.size() - 1)
            return track.type.isEmpty() ? track.values.get(lo) : track.type + "(" + track.values.get(lo) + ")";
        double t = (fraction - track.times[lo]) / (track.times[lo + 1] - track.times[lo]);
        if (track.mode.equals("spline")) t = ease(t, track.splines[lo]);
        String from = track.values.get(lo), to = track.values.get(lo + 1);
        if (COLORS.contains(track.name)) {
            float[] a = SvgImporter.color(from), b = SvgImporter.color(to);
            return "rgba(" + ((a[0] + (b[0] - a[0]) * t) * 255) + "," + ((a[1] + (b[1] - a[1]) * t) * 255) + "," +
                    ((a[2] + (b[2] - a[2]) * t) * 255) + "," + (a[3] + (b[3] - a[3]) * t) + ")";
        }
        boolean percent = from.endsWith("%");
        if (percent != to.endsWith("%")) throw error("Animated values must use matching units");
        double[] a = SvgImporter.numbers(percent ? from.substring(0, from.length() - 1) : from);
        double[] b = SvgImporter.numbers(percent ? to.substring(0, to.length() - 1) : to);
        if (a.length != b.length) throw error("Animated transform values must have matching arity");
        StringBuilder result = new StringBuilder();
        for (int i = 0; i < a.length; ++i) result.append(i == 0 ? "" : " ").append(a[i] + (b[i] - a[i]) * t);
        if (percent) result.append('%');
        return track.type.isEmpty() ? result.toString() : track.type + "(" + result + ")";
    }

    private static double ease(double x, double[] curve) {
        if (x <= 0 || x >= 1) return x;
        double lo = 0, hi = 1;
        for (int i = 0; i < 24; ++i) {
            double t = (lo + hi) * .5, u = 1 - t;
            double value = 3 * u * u * t * curve[0] + 3 * u * t * t * curve[2] + t * t * t;
            if (value < x) lo = t; else hi = t;
        }
        double t = (lo + hi) * .5, u = 1 - t;
        return 3 * u * u * t * curve[1] + 3 * u * t * t * curve[3] + t * t * t;
    }
}
