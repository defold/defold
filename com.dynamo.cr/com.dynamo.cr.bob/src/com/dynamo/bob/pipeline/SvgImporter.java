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

import com.dynamo.bob.slug.SlugCompiler;
import java.awt.geom.AffineTransform;
import java.awt.geom.Ellipse2D;
import java.awt.geom.NoninvertibleTransformException;
import java.awt.geom.Path2D;
import java.awt.geom.PathIterator;
import java.awt.geom.Rectangle2D;
import java.awt.geom.RoundRectangle2D;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import javax.xml.XMLConstants;
import javax.xml.parsers.DocumentBuilderFactory;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.xml.sax.InputSource;
import org.xml.sax.SAXParseException;
import org.xml.sax.helpers.DefaultHandler;

/** The fill-only SVG subset used by both Bob and editor vector resources. */
public final class SvgImporter {
    private SvgImporter() {}

    public record Stop(float offset, float r, float g, float b, float a) {}
    public record Paint(int type, int spread, float[] parameters, AffineTransform transform, List<Stop> stops) {}
    public record Layer(Path2D.Double path, Paint paint, boolean evenOdd) {}
    public record Image(float width, float height, List<Layer> layers) {}

    private static final Pattern NUMBER = Pattern.compile("[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?");
    private static final Set<String> PRESENTATION = Set.of("fill", "fill-opacity", "fill-rule", "stroke", "stroke-width",
            "opacity", "color", "display", "visibility", "stop-color", "stop-opacity", "color-interpolation",
            "clip-path", "mask", "filter", "marker-start", "marker-mid", "marker-end");
    private static final Set<String> HARMLESS_STYLE = Set.of("clip-rule", "stroke-linecap", "stroke-linejoin",
            "stroke-miterlimit", "stroke-opacity", "stroke-dasharray", "stroke-dashoffset", "enable-background",
            "shape-rendering", "isolation");
    // SVG 1.1 color keywords: https://www.w3.org/TR/SVG11/types.html#ColorKeywords
    private static final Map<String, String> COLORS = Map.ofEntries(
            Map.entry("aliceblue", "f0f8ff"),
            Map.entry("antiquewhite", "faebd7"),
            Map.entry("aqua", "00ffff"),
            Map.entry("aquamarine", "7fffd4"),
            Map.entry("azure", "f0ffff"),
            Map.entry("beige", "f5f5dc"),
            Map.entry("bisque", "ffe4c4"),
            Map.entry("black", "000000"),
            Map.entry("blanchedalmond", "ffebcd"),
            Map.entry("blue", "0000ff"),
            Map.entry("blueviolet", "8a2be2"),
            Map.entry("brown", "a52a2a"),
            Map.entry("burlywood", "deb887"),
            Map.entry("cadetblue", "5f9ea0"),
            Map.entry("chartreuse", "7fff00"),
            Map.entry("chocolate", "d2691e"),
            Map.entry("coral", "ff7f50"),
            Map.entry("cornflowerblue", "6495ed"),
            Map.entry("cornsilk", "fff8dc"),
            Map.entry("crimson", "dc143c"),
            Map.entry("cyan", "00ffff"),
            Map.entry("darkblue", "00008b"),
            Map.entry("darkcyan", "008b8b"),
            Map.entry("darkgoldenrod", "b8860b"),
            Map.entry("darkgray", "a9a9a9"),
            Map.entry("darkgreen", "006400"),
            Map.entry("darkgrey", "a9a9a9"),
            Map.entry("darkkhaki", "bdb76b"),
            Map.entry("darkmagenta", "8b008b"),
            Map.entry("darkolivegreen", "556b2f"),
            Map.entry("darkorange", "ff8c00"),
            Map.entry("darkorchid", "9932cc"),
            Map.entry("darkred", "8b0000"),
            Map.entry("darksalmon", "e9967a"),
            Map.entry("darkseagreen", "8fbc8f"),
            Map.entry("darkslateblue", "483d8b"),
            Map.entry("darkslategray", "2f4f4f"),
            Map.entry("darkslategrey", "2f4f4f"),
            Map.entry("darkturquoise", "00ced1"),
            Map.entry("darkviolet", "9400d3"),
            Map.entry("deeppink", "ff1493"),
            Map.entry("deepskyblue", "00bfff"),
            Map.entry("dimgray", "696969"),
            Map.entry("dimgrey", "696969"),
            Map.entry("dodgerblue", "1e90ff"),
            Map.entry("firebrick", "b22222"),
            Map.entry("floralwhite", "fffaf0"),
            Map.entry("forestgreen", "228b22"),
            Map.entry("fuchsia", "ff00ff"),
            Map.entry("gainsboro", "dcdcdc"),
            Map.entry("ghostwhite", "f8f8ff"),
            Map.entry("gold", "ffd700"),
            Map.entry("goldenrod", "daa520"),
            Map.entry("gray", "808080"),
            Map.entry("grey", "808080"),
            Map.entry("green", "008000"),
            Map.entry("greenyellow", "adff2f"),
            Map.entry("honeydew", "f0fff0"),
            Map.entry("hotpink", "ff69b4"),
            Map.entry("indianred", "cd5c5c"),
            Map.entry("indigo", "4b0082"),
            Map.entry("ivory", "fffff0"),
            Map.entry("khaki", "f0e68c"),
            Map.entry("lavender", "e6e6fa"),
            Map.entry("lavenderblush", "fff0f5"),
            Map.entry("lawngreen", "7cfc00"),
            Map.entry("lemonchiffon", "fffacd"),
            Map.entry("lightblue", "add8e6"),
            Map.entry("lightcoral", "f08080"),
            Map.entry("lightcyan", "e0ffff"),
            Map.entry("lightgoldenrodyellow", "fafad2"),
            Map.entry("lightgray", "d3d3d3"),
            Map.entry("lightgreen", "90ee90"),
            Map.entry("lightgrey", "d3d3d3"),
            Map.entry("lightpink", "ffb6c1"),
            Map.entry("lightsalmon", "ffa07a"),
            Map.entry("lightseagreen", "20b2aa"),
            Map.entry("lightskyblue", "87cefa"),
            Map.entry("lightslategray", "778899"),
            Map.entry("lightslategrey", "778899"),
            Map.entry("lightsteelblue", "b0c4de"),
            Map.entry("lightyellow", "ffffe0"),
            Map.entry("lime", "00ff00"),
            Map.entry("limegreen", "32cd32"),
            Map.entry("linen", "faf0e6"),
            Map.entry("magenta", "ff00ff"),
            Map.entry("maroon", "800000"),
            Map.entry("mediumaquamarine", "66cdaa"),
            Map.entry("mediumblue", "0000cd"),
            Map.entry("mediumorchid", "ba55d3"),
            Map.entry("mediumpurple", "9370db"),
            Map.entry("mediumseagreen", "3cb371"),
            Map.entry("mediumslateblue", "7b68ee"),
            Map.entry("mediumspringgreen", "00fa9a"),
            Map.entry("mediumturquoise", "48d1cc"),
            Map.entry("mediumvioletred", "c71585"),
            Map.entry("midnightblue", "191970"),
            Map.entry("mintcream", "f5fffa"),
            Map.entry("mistyrose", "ffe4e1"),
            Map.entry("moccasin", "ffe4b5"),
            Map.entry("navajowhite", "ffdead"),
            Map.entry("navy", "000080"),
            Map.entry("oldlace", "fdf5e6"),
            Map.entry("olive", "808000"),
            Map.entry("olivedrab", "6b8e23"),
            Map.entry("orange", "ffa500"),
            Map.entry("orangered", "ff4500"),
            Map.entry("orchid", "da70d6"),
            Map.entry("palegoldenrod", "eee8aa"),
            Map.entry("palegreen", "98fb98"),
            Map.entry("paleturquoise", "afeeee"),
            Map.entry("palevioletred", "db7093"),
            Map.entry("papayawhip", "ffefd5"),
            Map.entry("peachpuff", "ffdab9"),
            Map.entry("peru", "cd853f"),
            Map.entry("pink", "ffc0cb"),
            Map.entry("plum", "dda0dd"),
            Map.entry("powderblue", "b0e0e6"),
            Map.entry("purple", "800080"),
            Map.entry("red", "ff0000"),
            Map.entry("rosybrown", "bc8f8f"),
            Map.entry("royalblue", "4169e1"),
            Map.entry("saddlebrown", "8b4513"),
            Map.entry("salmon", "fa8072"),
            Map.entry("sandybrown", "f4a460"),
            Map.entry("seagreen", "2e8b57"),
            Map.entry("seashell", "fff5ee"),
            Map.entry("sienna", "a0522d"),
            Map.entry("silver", "c0c0c0"),
            Map.entry("skyblue", "87ceeb"),
            Map.entry("slateblue", "6a5acd"),
            Map.entry("slategray", "708090"),
            Map.entry("slategrey", "708090"),
            Map.entry("snow", "fffafa"),
            Map.entry("springgreen", "00ff7f"),
            Map.entry("steelblue", "4682b4"),
            Map.entry("tan", "d2b48c"),
            Map.entry("teal", "008080"),
            Map.entry("thistle", "d8bfd8"),
            Map.entry("tomato", "ff6347"),
            Map.entry("turquoise", "40e0d0"),
            Map.entry("violet", "ee82ee"),
            Map.entry("wheat", "f5deb3"),
            Map.entry("white", "ffffff"),
            Map.entry("whitesmoke", "f5f5f5"),
            Map.entry("yellow", "ffff00"),
            Map.entry("yellowgreen", "9acd32"));

    record Vector(Image image, SvgAnimation animation) {}

    static Vector parseVector(byte[] bytes) throws IOException {
        SvgAnimation animation = new SvgAnimation(readRoot(bytes));
        return new Vector(parse(animation.root, animation.animated() ? animation : null), animation);
    }

    public static Image parse(byte[] bytes) throws IOException {
        return parseAtTime(bytes, 0);
    }

    // Used by the SVG source preview and independent animation-reference tests.
    static Image parseAtTime(byte[] bytes, double time) throws IOException {
        SvgAnimation animation = new SvgAnimation(readRoot(bytes));
        return parse(animation.sample(time));
    }

    private static Element readRoot(byte[] bytes) throws IOException {
        try {
            DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
            factory.setNamespaceAware(true);
            factory.setFeature(XMLConstants.FEATURE_SECURE_PROCESSING, true);
            factory.setFeature("http://xml.org/sax/features/external-general-entities", false);
            factory.setFeature("http://xml.org/sax/features/external-parameter-entities", false);
            factory.setFeature("http://apache.org/xml/features/nonvalidating/load-external-dtd", false);
            factory.setAttribute(XMLConstants.ACCESS_EXTERNAL_DTD, "");
            factory.setAttribute(XMLConstants.ACCESS_EXTERNAL_SCHEMA, "");
            factory.setExpandEntityReferences(false);
            var builder = factory.newDocumentBuilder();
            builder.setEntityResolver((publicId, systemId) -> new InputSource(new StringReader("")));
            builder.setErrorHandler(new DefaultHandler() {
                @Override public void error(SAXParseException e) throws SAXParseException { throw e; }
                @Override public void fatalError(SAXParseException e) throws SAXParseException { throw e; }
            });
            var document = builder.parse(new ByteArrayInputStream(bytes));
            for (Node node = document.getFirstChild(); node != null; node = node.getNextSibling()) {
                if (node.getNodeType() == Node.PROCESSING_INSTRUCTION_NODE && node.getNodeName().equals("xml-stylesheet"))
                    throw new IOException("SVG stylesheets are unsupported; use inline styles or presentation attributes");
            }
            Element root = document.getDocumentElement();
            if (!"svg".equals(root.getLocalName())) throw error(root, "Expected an SVG document");
            return root;
        } catch (IOException e) {
            throw e;
        } catch (Exception e) {
            throw new IOException("Invalid SVG: " + e.getMessage(), e);
        }
    }

    private static Image parse(Element root) throws IOException {
        return parse(root, null);
    }

    private static Image parse(Element root, SvgAnimation animation) throws IOException {
        double[] box = root.hasAttribute("viewBox") ? numbers(root.getAttribute("viewBox")) : null;
        if (box != null && (box.length != 4 || box[2] <= 0 || box[3] <= 0)) throw error(root, "Invalid viewBox");
        double width = rootLength(root, "width", box == null ? 0 : box[2]);
        double height = rootLength(root, "height", box == null ? 0 : box[3]);
        if (!(width > 0 && height > 0 && width <= 65535 && height <= 65535))
            throw error(root, "SVG dimensions must be positive and at most 65535");
        AffineTransform viewport = new AffineTransform();
        if (box != null) {
            double sx = width / box[2], sy = height / box[3];
            String aspect = root.getAttribute("preserveAspectRatio").trim();
            if (aspect.isEmpty()) aspect = "xMidYMid meet";
            double tx = 0, ty = 0;
            if (!aspect.equals("none")) {
                String[] parts = aspect.split("\\s+");
                if (!parts[0].matches("x(Min|Mid|Max)Y(Min|Mid|Max)") || parts.length > 2 ||
                        (parts.length == 2 && !parts[1].equals("meet") && !parts[1].equals("slice")))
                    throw error(root, "Unsupported preserveAspectRatio");
                sx = sy = parts.length == 2 && parts[1].equals("slice") ? Math.max(sx, sy) : Math.min(sx, sy);
                tx = (width - box[2] * sx) * (parts[0].contains("xMid") ? .5 : parts[0].contains("xMax") ? 1 : 0);
                ty = (height - box[3] * sy) * (parts[0].contains("YMid") ? .5 : parts[0].contains("YMax") ? 1 : 0);
            }
            viewport.translate(tx, ty);
            viewport.scale(sx, sy);
            viewport.translate(-box[0], -box[1]);
        }
        if (animation != null) animation.viewport = new AffineTransform(viewport);
        Map<String, Element> ids = new HashMap<>();
        collectIds(root, ids);
        List<Layer> layers = new ArrayList<>();
        walk(root, Map.of(), viewport, ids, box == null ? width : box[2], box == null ? height : box[3], layers, true, animation);
        return new Image((float)width, (float)height, List.copyOf(layers));
    }

    /** Converts parsed geometry to the shared native Slug compiler input. */
    public static SlugCompiler.Image toVectorImage(Image image) throws IOException {
        SlugCompiler.Path[] paths = new SlugCompiler.Path[image.layers.size()];
        for (int i = 0; i < paths.length; ++i) {
            Layer layer = image.layers.get(i);
            List<Float> commands = new ArrayList<>();
            double[] points = new double[6];
            for (PathIterator it = layer.path.getPathIterator(null); !it.isDone(); it.next()) {
                java.util.Arrays.fill(points, 0);
                int type = it.currentSegment(points);
                commands.add((float)type);
                for (double point : points) {
                    float value = (float)point;
                    if (!Float.isFinite(value)) throw new IOException("SVG coordinates exceed the supported numeric range");
                    commands.add(value);
                }
            }
            float[] commandData = new float[commands.size()];
            for (int j = 0; j < commandData.length; ++j) commandData[j] = commands.get(j);
            Paint paint = layer.paint;
            float[] matrix = new float[6];
            double[] affine = new double[6];
            paint.transform.getMatrix(affine);
            for (int j = 0; j < 6; ++j) matrix[j] = (float)affine[j];
            float[] stops = new float[paint.stops.size() * 5];
            for (int j = 0; j < paint.stops.size(); ++j) {
                Stop stop = paint.stops.get(j);
                int offset = j * 5;
                stops[offset] = stop.offset; stops[offset + 1] = stop.r; stops[offset + 2] = stop.g;
                stops[offset + 3] = stop.b; stops[offset + 4] = stop.a;
            }
            paths[i] = new SlugCompiler.Path(commandData, layer.evenOdd, paint.type, paint.spread, paint.parameters, matrix, stops);
        }
        return new SlugCompiler.Image(image.width, image.height, paths);
    }

    private static IOException error(Element element, String message) {
        return new IOException(message + " in <" + element.getTagName() + ">" +
                (element.hasAttribute("id") ? " #" + element.getAttribute("id") : ""));
    }

    private static void collectIds(Element e, Map<String, Element> ids) throws IOException {
        if (Set.of("style", "text", "tspan", "image", "use", "symbol", "clipPath", "mask", "filter",
                "animate", "animateTransform", "animateMotion", "set", "foreignObject").contains(e.getLocalName()))
            throw error(e, "Unsupported SVG element; convert it to filled paths");
        if (e.hasAttribute("id") && ids.put(e.getAttribute("id"), e) != null) throw error(e, "Duplicate id");
        for (Node child = e.getFirstChild(); child != null; child = child.getNextSibling())
            if (child instanceof Element element) collectIds(element, ids);
    }

    private static Map<String, String> style(Element e, Map<String, String> inherited) throws IOException {
        Map<String, String> result = new HashMap<>(inherited);
        result.remove("opacity"); // Opacity composites an element; it is not inherited.
        for (String key : PRESENTATION) if (e.hasAttribute(key)) result.put(key, e.getAttribute(key).trim());
        for (String declaration : e.getAttribute("style").split(";")) {
            if (declaration.isBlank()) continue;
            int colon = declaration.indexOf(':');
            if (colon < 0) throw error(e, "Invalid style declaration");
            String key = declaration.substring(0, colon).trim();
            String value = declaration.substring(colon + 1).trim();
            if (!PRESENTATION.contains(key) && !HARMLESS_STYLE.contains(key)) throw error(e, "Unsupported style property: " + key);
            result.put(key, value);
        }
        for (String key : List.copyOf(result.keySet()))
            if (result.get(key).equals("inherit")) {
                if (inherited.containsKey(key)) result.put(key, inherited.get(key)); else result.remove(key);
            }
        for (String key : List.of("clip-path", "mask", "filter", "marker-start", "marker-mid", "marker-end"))
            if (!result.getOrDefault(key, "none").equals("none")) throw error(e, "Unsupported SVG property: " + key);
        if (!Set.of("sRGB", "auto").contains(result.getOrDefault("color-interpolation", "sRGB")))
            throw error(e, "Only sRGB color-interpolation is supported for SVG");
        return result;
    }

    private static void walk(Element e, Map<String, String> inherited, AffineTransform parent, Map<String, Element> ids,
                             double width, double height, List<Layer> layers, boolean root, SvgAnimation animation) throws IOException {
        String tag = e.getLocalName();
        if (Set.of("title", "desc", "metadata", "defs", "linearGradient", "radialGradient").contains(tag)) return;
        Map<String, String> s = style(e, inherited);
        if (animation == null && s.getOrDefault("display", "inline").equals("none")) return;
        AffineTransform transform = new AffineTransform(parent);
        if (animation == null) transform.concatenate(transform(e.getAttribute("transform")));
        if (tag.equals("g") || (root && tag.equals("svg"))) {
            if (number(s.getOrDefault("opacity", "1")) != 1) throw error(e, "Group opacity is unsupported; use fill-opacity");
            for (Node child = e.getFirstChild(); child != null; child = child.getNextSibling())
                if (child instanceof Element element) walk(element, s, transform, ids, width, height, layers, false, animation);
            return;
        }
        Path2D.Double path = shape(e, width, height);
        if (!s.getOrDefault("stroke", "none").equals("none") && length(s.getOrDefault("stroke-width", "1"), width) != 0)
            throw error(e, "Strokes are unsupported; convert the stroke to a filled path");
        if ((animation == null && s.getOrDefault("visibility", "visible").equals("hidden")) || (animation == null && s.getOrDefault("fill", "black").equals("none"))) return;
        String fillRule = s.getOrDefault("fill-rule", "nonzero");
        if (!Set.of("nonzero", "evenodd").contains(fillRule)) throw error(e, "Invalid fill-rule");
        Rectangle2D localBounds = bounds(path);
        if (localBounds.getWidth() <= 0 || localBounds.getHeight() <= 0 || transform.getDeterminant() == 0) return;
        if (animation != null) {
            // Normalize in double precision before crossing the float compiler bridge.
            // The node's timeline places this one canonical path in the SVG viewport.
            transform = new AffineTransform();
            transform.scale(1.0 / localBounds.getWidth(), 1.0 / localBounds.getHeight());
            transform.translate(-localBounds.getX(), -localBounds.getY());
        }
        Map<String, String> paintStyle = s;
        if (animation != null && s.getOrDefault("fill", "black").equals("none")) {
            paintStyle = new HashMap<>(s);
            paintStyle.put("fill", "transparent");
        }
        Paint paint = paint(e, paintStyle, ids, transform, localBounds, width, height, animation != null);
        path.transform(transform);
        Rectangle2D bounds = bounds(path);
        if (bounds.getWidth() > 0 && bounds.getHeight() > 0 && !paint.stops().isEmpty()) {
            layers.add(new Layer(path, paint, fillRule.equals("evenodd")));
            if (animation != null) animation.layers.add(new SvgAnimation.Layer(e, new AffineTransform(transform), width, height));
        }
    }

    private static Path2D.Double shape(Element e, double width, double height) throws IOException {
        String tag = e.getLocalName();
        if (tag.equals("path")) return new PathReader(e.getAttribute("d")).parse();
        Path2D.Double path = new Path2D.Double();
        double x = attr(e, "x", 0, width), y = attr(e, "y", 0, height);
        switch (tag) {
            case "rect": {
                double w = attr(e, "width", 0, width), h = attr(e, "height", 0, height);
                double rx = attr(e, "rx", 0, width), ry = attr(e, "ry", rx, height);
                if (!e.hasAttribute("rx")) rx = ry;
                if (w < 0 || h < 0 || rx < 0 || ry < 0) throw error(e, "Negative rectangle dimension");
                path.append(new RoundRectangle2D.Double(x, y, w, h, Math.min(2 * rx, w), Math.min(2 * ry, h)), false);
                break;
            }
            case "circle": case "ellipse": {
                double cx = attr(e, "cx", 0, width), cy = attr(e, "cy", 0, height);
                double rx = tag.equals("circle") ? attr(e, "r", 0, Math.hypot(width, height) / Math.sqrt(2)) : attr(e, "rx", 0, width);
                double ry = tag.equals("circle") ? rx : attr(e, "ry", 0, height);
                if (rx < 0 || ry < 0) throw error(e, "Negative ellipse radius");
                path.append(new Ellipse2D.Double(cx - rx, cy - ry, 2 * rx, 2 * ry), false);
                break;
            }
            case "polyline": case "polygon": {
                double[] p = numbers(e.getAttribute("points"));
                if (p.length % 2 != 0) throw error(e, "Expected coordinate pairs");
                if (p.length != 0) {
                    path.moveTo(p[0], p[1]);
                    for (int i = 2; i < p.length; i += 2) path.lineTo(p[i], p[i + 1]);
                    path.closePath();
                }
                break;
            }
            case "line": break; // A line has no fill area. Visible strokes are rejected by the caller.
            default: throw error(e, "Unsupported SVG element");
        }
        return path;
    }

    static Map<String, String> inheritedStyle(Element element) throws IOException {
        Map<String, String> inherited = element.getParentNode() instanceof Element parent ? inheritedStyle(parent) : Map.of();
        return style(element, inherited);
    }

    static Map<String, String> gradient(Element e, Map<String, Element> ids, Set<Element> active, List<Element> stops) throws IOException {
        if (!active.add(e)) throw error(e, "Cyclic gradient reference");
        Map<String, String> values = new LinkedHashMap<>();
        String href = e.hasAttribute("href") ? e.getAttribute("href") : e.getAttributeNS("http://www.w3.org/1999/xlink", "href");
        if (!href.isEmpty()) {
            Element other = href.startsWith("#") ? ids.get(href.substring(1)) : null;
            if (other == null || !Set.of("linearGradient", "radialGradient").contains(other.getLocalName()))
                throw error(e, "Missing or external gradient reference: " + href);
            values.putAll(gradient(other, ids, active, stops));
        }
        var attributes = e.getAttributes();
        for (int i = 0; i < attributes.getLength(); i++) values.put(attributes.item(i).getNodeName(), attributes.item(i).getNodeValue());
        List<Element> ownStops = new ArrayList<>();
        for (Node child = e.getFirstChild(); child != null; child = child.getNextSibling())
            if (child instanceof Element element) {
                if (!element.getLocalName().equals("stop")) throw error(element, "Unsupported gradient child");
                ownStops.add(element);
            }
        if (!ownStops.isEmpty()) { stops.clear(); stops.addAll(ownStops); }
        active.remove(e);
        return values;
    }

    private static Paint paint(Element e, Map<String, String> style, Map<String, Element> ids, AffineTransform shapeTransform,
                               Rectangle2D bounds, double width, double height, boolean animated) throws IOException {
        String fill = style.getOrDefault("fill", "black");
        float opacity = animated ? 1 : (float)(unit(style.getOrDefault("fill-opacity", "1")) * unit(style.getOrDefault("opacity", "1")));
        if (!fill.startsWith("url(")) {
            float[] color = color(fill.equals("currentColor") ? style.getOrDefault("color", "black") : fill);
            return new Paint(0, 0, new float[6], new AffineTransform(), List.of(new Stop(0, color[0], color[1], color[2], opacity * color[3])));
        }
        if (!fill.endsWith(")")) throw error(e, "Invalid paint reference: " + fill);
        String reference = fill.substring(4, fill.length() - 1).trim().replace("\"", "").replace("'", "");
        Element gradient = reference.startsWith("#") ? ids.get(reference.substring(1)) : null;
        if (gradient == null || !Set.of("linearGradient", "radialGradient").contains(gradient.getLocalName()))
            throw error(e, "Missing or unsupported paint reference: " + fill);
        List<Element> stopElements = new ArrayList<>();
        Map<String, String> g = gradient(gradient, ids, new HashSet<>(), stopElements);
        boolean object = g.getOrDefault("gradientUnits", "objectBoundingBox").equals("objectBoundingBox");
        if (!object && !g.get("gradientUnits").equals("userSpaceOnUse")) throw error(gradient, "Invalid gradientUnits");
        AffineTransform matrix = new AffineTransform(shapeTransform);
        if (object) {
            matrix.translate(bounds.getX(), bounds.getY());
            matrix.scale(bounds.getWidth(), bounds.getHeight());
        }
        matrix.concatenate(transform(g.getOrDefault("gradientTransform", "")));
        try { matrix.invert(); } catch (NoninvertibleTransformException ex) { throw error(e, "Singular gradient transform"); }
        double w = object ? 1 : width, h = object ? 1 : height;
        float[] p = new float[6];
        int type = gradient.getLocalName().equals("linearGradient") ? 1 : 2;
        if (type == 1) {
            p[0] = (float)length(g.getOrDefault("x1", "0%"), w); p[1] = (float)length(g.getOrDefault("y1", "0%"), h);
            p[2] = (float)length(g.getOrDefault("x2", "100%"), w); p[3] = (float)length(g.getOrDefault("y2", "0%"), h);
        } else {
            p[3] = (float)length(g.getOrDefault("cx", "50%"), w); p[4] = (float)length(g.getOrDefault("cy", "50%"), h);
            p[0] = (float)length(g.getOrDefault("fx", g.getOrDefault("cx", "50%")), w);
            p[1] = (float)length(g.getOrDefault("fy", g.getOrDefault("cy", "50%")), h);
            p[5] = (float)length(g.getOrDefault("r", "50%"), Math.hypot(w, h) / Math.sqrt(2));
            if (p[5] < 0) throw error(gradient, "Negative gradient radius");
            double distance = Math.hypot(p[0] - p[3], p[1] - p[4]);
            if (distance > p[5]) {
                p[0] = (float)(p[3] + (p[0] - p[3]) * p[5] / distance);
                p[1] = (float)(p[4] + (p[1] - p[4]) * p[5] / distance);
            }
            if (g.containsKey("fr") && number(g.get("fr")) != 0) throw error(gradient, "SVG radial gradient fr is unsupported");
        }
        // Keep the complete gradient in the canonical image. Runtime tracks can
        // turn a degenerate gradient into a nondegenerate one (and vice versa).
        if (animated && type == 1 && p[0] == p[2] && p[1] == p[3]) p[2] = p[0] + 1;
        if (animated && type == 2 && p[5] == 0) p[5] = 1;
        int spread = switch (g.getOrDefault("spreadMethod", "pad")) {
            case "pad" -> 0; case "repeat" -> 1; case "reflect" -> 2;
            default -> throw error(gradient, "Invalid spreadMethod");
        };
        List<Stop> stops = new ArrayList<>();
        float previous = 0;
        for (Element stop : stopElements) {
            Map<String, String> s = inheritedStyle(stop);
            float offset = Math.max(previous, (float)(stop.hasAttribute("offset") ? unit(stop.getAttribute("offset")) : 0));
            String c = s.getOrDefault("stop-color", "black");
            float[] color = color(c.equals("currentColor") ? s.getOrDefault("color", "black") : c);
            stops.add(new Stop(offset, color[0], color[1], color[2], color[3] * opacity * (animated ? 1 : (float)unit(s.getOrDefault("stop-opacity", "1")))));
            previous = offset;
        }
        if (stops.isEmpty()) stops.add(new Stop(0, 0, 0, 0, 0));
        return new Paint(type, spread, p, matrix, List.copyOf(stops));
    }

    static float[] color(String value) throws IOException {
        value = value.trim().toLowerCase(java.util.Locale.ROOT);
        if (value.equals("transparent")) return new float[4];
        if (COLORS.containsKey(value)) value = "#" + COLORS.get(value);
        try {
            if (value.startsWith("#")) {
                String hex = value.substring(1);
                if (hex.length() == 3) hex = "" + hex.charAt(0) + hex.charAt(0) + hex.charAt(1) + hex.charAt(1) + hex.charAt(2) + hex.charAt(2);
                if (hex.length() != 6) throw new NumberFormatException();
                int rgb = Integer.parseInt(hex, 16);
                return new float[]{((rgb >> 16) & 255) / 255f, ((rgb >> 8) & 255) / 255f, (rgb & 255) / 255f, 1};
            }
            if ((value.startsWith("rgb(") || value.startsWith("rgba(")) && value.endsWith(")")) {
                boolean alpha = value.startsWith("rgba(");
                String[] parts = value.substring(alpha ? 5 : 4, value.length() - 1).trim().split("[\\s,]+");
                if (parts.length != (alpha ? 4 : 3)) throw new NumberFormatException();
                float[] c = new float[]{0, 0, 0, 1};
                for (int i = 0; i < 3; i++) c[i] = (float)Math.max(0, Math.min(1, length(parts[i], 255) / 255));
                if (alpha) c[3] = (float)unit(parts[3]);
                return c;
            }
        } catch (NumberFormatException ignored) {}
        throw new IOException("Unsupported SVG color: " + value);
    }

    private static double rootLength(Element e, String name, double fallback) throws IOException {
        String value = e.getAttribute(name).trim();
        return value.isEmpty() || value.endsWith("%") ? fallback : length(value, fallback);
    }
    private static double attr(Element e, String name, double fallback, double reference) throws IOException {
        return e.hasAttribute(name) ? length(e.getAttribute(name), reference) : fallback;
    }
    private static double unit(String s) throws IOException { return Math.max(0, Math.min(1, length(s, 1))); }
    static double number(String s) throws IOException {
        double[] values = numbers(s);
        if (values.length != 1) throw new IOException("Expected one SVG number: " + s);
        return values[0];
    }
    static double length(String s, double reference) throws IOException {
        s = s.trim();
        Matcher m = NUMBER.matcher(s);
        if (!m.lookingAt()) throw new IOException("Invalid SVG length: " + s);
        double v = Double.parseDouble(m.group());
        double scale = switch (s.substring(m.end()).trim()) {
            case "", "px" -> 1; case "%" -> reference / 100; case "in" -> 96;
            case "cm" -> 96 / 2.54; case "mm" -> 96 / 25.4; case "pt" -> 96 / 72.0; case "pc" -> 16;
            default -> throw new IOException("Unsupported SVG length unit: " + s);
        };
        if (!Double.isFinite(v * scale)) throw new IOException("Non-finite SVG length");
        return v * scale;
    }
    static double[] numbers(String s) throws IOException {
        Scanner scan = new Scanner(s);
        List<Double> values = new ArrayList<>();
        while (scan.more()) values.add(scan.number());
        return values.stream().mapToDouble(Double::doubleValue).toArray();
    }
    static AffineTransform transform(String text) throws IOException {
        AffineTransform result = new AffineTransform();
        int cursor = 0;
        while (cursor < text.length()) {
            while (cursor < text.length() && (Character.isWhitespace(text.charAt(cursor)) || text.charAt(cursor) == ',')) cursor++;
            if (cursor == text.length()) break;
            int open = text.indexOf('(', cursor), close = text.indexOf(')', open + 1);
            if (open < 0 || close < 0) throw new IOException("Invalid SVG transform");
            String name = text.substring(cursor, open).trim();
            double[] p = numbers(text.substring(open + 1, close));
            switch (name) {
                case "matrix": if (p.length != 6) throw new IOException("matrix requires six values"); result.concatenate(new AffineTransform(p)); break;
                case "translate": if (p.length < 1 || p.length > 2) throw new IOException("translate requires one or two values"); result.translate(p[0], p.length == 2 ? p[1] : 0); break;
                case "scale": if (p.length < 1 || p.length > 2) throw new IOException("scale requires one or two values"); result.scale(p[0], p.length == 2 ? p[1] : p[0]); break;
                case "rotate": if (p.length != 1 && p.length != 3) throw new IOException("rotate requires one or three values"); result.rotate(Math.toRadians(p[0]), p.length == 3 ? p[1] : 0, p.length == 3 ? p[2] : 0); break;
                case "skewX": case "skewY": if (p.length != 1) throw new IOException("skew requires one value"); result.shear(name.equals("skewX") ? Math.tan(Math.toRadians(p[0])) : 0, name.equals("skewY") ? Math.tan(Math.toRadians(p[0])) : 0); break;
                default: throw new IOException("Unsupported SVG transform: " + name);
            }
            cursor = close + 1;
        }
        return result;
    }

    // Exact Bezier extrema matter for objectBoundingBox gradients.
    private static Rectangle2D bounds(Path2D.Double path) {
        double[] p = new double[6];
        double x = 0, y = 0, sx = 0, sy = 0;
        Rectangle2D.Double box = null;
        for (PathIterator it = path.getPathIterator(null); !it.isDone(); it.next()) {
            int type = it.currentSegment(p);
            if (type == PathIterator.SEG_CLOSE) { x = sx; y = sy; continue; }
            int end = type == PathIterator.SEG_CUBICTO ? 4 : type == PathIterator.SEG_QUADTO ? 2 : 0;
            if (box == null) box = new Rectangle2D.Double(p[end], p[end + 1], 0, 0);
            box.add(p[end], p[end + 1]);
            if (type == PathIterator.SEG_MOVETO) { sx = p[0]; sy = p[1]; }
            if (type == PathIterator.SEG_QUADTO || type == PathIterator.SEG_CUBICTO) {
                for (int axis = 0; axis < 2; axis++) {
                    double a0 = axis == 0 ? x : y, a1 = p[axis], a2 = p[axis + 2];
                    double a = 0, b = a0 - 2 * a1 + a2, c = a1 - a0;
                    if (type == PathIterator.SEG_CUBICTO) {
                        a = -a0 + 3 * a1 - 3 * a2 + p[axis + 4]; b *= 2;
                    }
                    double discriminant = b * b - 4 * a * c;
                    double[] roots = Math.abs(a) < 1e-12 ? new double[]{b == 0 ? -1 : -c / b}
                            : discriminant < 0 ? new double[0] : new double[]{(-b + Math.sqrt(discriminant)) / (2 * a), (-b - Math.sqrt(discriminant)) / (2 * a)};
                    for (double t : roots) if (t > 0 && t < 1) {
                        double u = 1 - t;
                        if (type == PathIterator.SEG_QUADTO) box.add(u*u*x + 2*u*t*p[0] + t*t*p[2], u*u*y + 2*u*t*p[1] + t*t*p[3]);
                        else box.add(u*u*u*x + 3*u*u*t*p[0] + 3*u*t*t*p[2] + t*t*t*p[4], u*u*u*y + 3*u*u*t*p[1] + 3*u*t*t*p[3] + t*t*t*p[5]);
                    }
                }
            }
            x = p[end]; y = p[end + 1];
        }
        return box == null ? new Rectangle2D.Double() : box;
    }

    private static class Scanner {
        final String text;
        int at;
        Scanner(String text) { this.text = text; }
        boolean more() {
            while (at < text.length() && (Character.isWhitespace(text.charAt(at)) || text.charAt(at) == ',')) at++;
            return at < text.length();
        }
        double number() throws IOException {
            more();
            Matcher m = NUMBER.matcher(text).region(at, text.length());
            if (!m.lookingAt()) throw new IOException("Expected SVG number at: " + text.substring(at, Math.min(at + 24, text.length())));
            at = m.end();
            double value = Double.parseDouble(m.group());
            if (!Double.isFinite(value)) throw new IOException("Non-finite SVG coordinate");
            return value;
        }
        boolean flag() throws IOException {
            if (!more() || (text.charAt(at) != '0' && text.charAt(at) != '1')) throw new IOException("Expected SVG arc flag 0 or 1");
            return text.charAt(at++) == '1';
        }
    }

    private static final class PathReader extends Scanner {
        final Path2D.Double path = new Path2D.Double();
        double x, y, sx, sy, cx, cy;
        boolean active;
        PathReader(String text) { super(text); }
        Path2D.Double parse() throws IOException {
            char command = 0, previous = 0;
            while (more()) {
                if (Character.isLetter(text.charAt(at))) command = text.charAt(at++);
                else if (command == 0 || command == 'z' || command == 'Z') throw new IOException("Missing SVG path command");
                char op = Character.toUpperCase(command);
                boolean relative = Character.isLowerCase(command);
                double ox = relative ? x : 0, oy = relative ? y : 0;
                if (!active && op != 'M') throw new IOException("SVG path must start with moveto");
                // SVG permits continuing from the closed contour's start point.
                // Slug needs an explicit move to begin that next contour.
                if (previous == 'Z' && op != 'M' && op != 'Z') path.moveTo(x, y);
                switch (op) {
                    case 'M':
                        if (active) path.closePath();
                        x = ox + number(); y = oy + number(); sx = x; sy = y;
                        path.moveTo(x, y); active = true; command = relative ? 'l' : 'L'; break;
                    case 'L': x = ox + number(); y = oy + number(); path.lineTo(x, y); break;
                    case 'H': x = ox + number(); path.lineTo(x, y); break;
                    case 'V': y = oy + number(); path.lineTo(x, y); break;
                    case 'C': {
                        double x1 = ox + number(), y1 = oy + number(); cx = ox + number(); cy = oy + number();
                        x = ox + number(); y = oy + number(); path.curveTo(x1, y1, cx, cy, x, y); break;
                    }
                    case 'S': {
                        double x1 = previous == 'C' || previous == 'S' ? 2*x - cx : x;
                        double y1 = previous == 'C' || previous == 'S' ? 2*y - cy : y;
                        cx = ox + number(); cy = oy + number(); x = ox + number(); y = oy + number();
                        path.curveTo(x1, y1, cx, cy, x, y); break;
                    }
                    case 'Q': cx = ox + number(); cy = oy + number(); x = ox + number(); y = oy + number(); path.quadTo(cx, cy, x, y); break;
                    case 'T': cx = previous == 'Q' || previous == 'T' ? 2*x - cx : x; cy = previous == 'Q' || previous == 'T' ? 2*y - cy : y;
                        x = ox + number(); y = oy + number(); path.quadTo(cx, cy, x, y); break;
                    case 'A': {
                        double rx = number(), ry = number(), angle = number(); boolean large = flag(), sweep = flag();
                        double nx = ox + number(), ny = oy + number(); arc(rx, ry, angle, large, sweep, nx, ny); x = nx; y = ny; break;
                    }
                    case 'Z': path.closePath(); x = sx; y = sy; break;
                    default: throw new IOException("Unsupported SVG path command: " + command);
                }
                previous = op;
            }
            if (active) path.closePath();
            return path;
        }
        private void arc(double rx, double ry, double angle, boolean large, boolean sweep, double nx, double ny) {
            rx = Math.abs(rx); ry = Math.abs(ry);
            if (x == nx && y == ny) return;
            if (rx == 0 || ry == 0) { path.lineTo(nx, ny); return; }
            double phi = Math.toRadians(angle), c = Math.cos(phi), s = Math.sin(phi);
            double px = c*(x-nx)/2 + s*(y-ny)/2, py = -s*(x-nx)/2 + c*(y-ny)/2;
            double scale = px*px/(rx*rx) + py*py/(ry*ry);
            if (scale > 1) { rx *= Math.sqrt(scale); ry *= Math.sqrt(scale); }
            double f = Math.sqrt(Math.max(0, (rx*rx*ry*ry - rx*rx*py*py - ry*ry*px*px) / (rx*rx*py*py + ry*ry*px*px)));
            if (large == sweep) f = -f;
            double qx = f*rx*py/ry, qy = -f*ry*px/rx;
            double centerX = c*qx-s*qy+(x+nx)/2, centerY = s*qx+c*qy+(y+ny)/2;
            double start = Math.atan2((py-qy)/ry, (px-qx)/rx);
            double delta = Math.atan2((-py-qy)/ry, (-px-qx)/rx) - start;
            if (sweep && delta < 0) delta += 2*Math.PI;
            if (!sweep && delta > 0) delta -= 2*Math.PI;
            int count = Math.max(1, (int)Math.ceil(Math.abs(delta)/(Math.PI/2)));
            for (int i = 0; i < count; i++) {
                double a = start + delta*i/count, b = start + delta*(i+1)/count;
                double k = 4.0/3 * Math.tan((b-a)/4);
                double[] p = {Math.cos(a)-k*Math.sin(a), Math.sin(a)+k*Math.cos(a), Math.cos(b)+k*Math.sin(b), Math.sin(b)-k*Math.cos(b), Math.cos(b), Math.sin(b)};
                for (int j = 0; j < 6; j += 2) {
                    double u = p[j]*rx, v = p[j+1]*ry;
                    p[j] = centerX+c*u-s*v; p[j+1] = centerY+s*u+c*v;
                }
                path.curveTo(p[0], p[1], p[2], p[3], p[4], p[5]);
            }
        }
    }
}
