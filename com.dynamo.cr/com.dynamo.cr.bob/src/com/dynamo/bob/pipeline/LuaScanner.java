package com.dynamo.bob.pipeline;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import com.dynamo.bob.pipeline.LuaScanner.Property.Status;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.gameobject.proto.GameObject.PropertyType;

public class LuaScanner {

    private static Pattern multiLineCommentPattern = Pattern.compile("--\\[\\[.*?--\\]\\]",
            Pattern.DOTALL | Pattern.MULTILINE);

    private static String comment = "\\s*?(-{2,}.*?)?$";
    private static String identifier = "[_\\p{L}][_\\p{L}0-9]*";
    private static String beforeRequire = ".*?";
    private static String afterRequire = "\\s*(,{0,1}|\\." + identifier + ",{0,1})" + comment;
    
    
    private static Pattern requirePattern1 = Pattern.compile(beforeRequire + "require\\s*?\"(.*?)\"" + afterRequire,
             Pattern.DOTALL | Pattern.MULTILINE);

    private static Pattern requirePattern2 = Pattern.compile(beforeRequire + "require\\s*?\\(\\s*?\"(.*?)\"\\s*?\\)" + afterRequire,
             Pattern.DOTALL | Pattern.MULTILINE);

    private static Pattern requirePattern3 = Pattern.compile(beforeRequire + "require\\s*?'(.*?)'" + afterRequire,
             Pattern.DOTALL | Pattern.MULTILINE);

    /**
     * Note: we need four different patterns here to match the same beginning and ending character for
     * a string (eg " or '). We can't match on [\"']. If we do we'd have a false positive for the
     * following line:
     * 
     * local s = 'require "should_not_match"'
     */
    private static Pattern requirePattern4 = Pattern.compile(beforeRequire + "require\\s*?\\(\\s*?'(.*?)'\\s*?\\)" + afterRequire,
             Pattern.DOTALL | Pattern.MULTILINE);

    private static Pattern propertyDeclPattern = Pattern.compile("go.property\\((.*?)\\);?(\\s*?--.*?)?$");
    private static Pattern propertyArgsPattern = Pattern.compile("[\"'](.*?)[\"']\\s*,(.*)");

    // http://docs.python.org/dev/library/re.html#simulating-scanf
    private static Pattern numPattern = Pattern.compile("[-+]?(\\d+(\\.\\d*)?|\\.\\d+)([eE][-+]?\\d+)?");
    private static Pattern hashPattern = Pattern.compile("hash\\s*\\([\"'](.*?)[\"']\\)");
    private static Pattern urlPattern = Pattern.compile("msg\\.url\\s*\\(([\"'](.*?)[\"']|)?\\)");
    private static Pattern vec3Pattern = Pattern.compile("vmath\\.vector3\\s*\\(((.*?),(.*?),(.*?)|)\\)");
    private static Pattern vec4Pattern = Pattern.compile("vmath\\.vector4\\s*\\(((.*?),(.*?),(.*?),(.*?)|)\\)");
    private static Pattern quatPattern = Pattern.compile("vmath\\.quat\\s*\\(((.*?),(.*?),(.*?),(.*?)|)\\)");
    private static Pattern boolPattern = Pattern.compile("(false|true)");
    private static Pattern materialPattern = Pattern.compile("material\\s*\\(([\"'](.*?)[\"']|)?\\)");
    private static Pattern textureSetPattern = Pattern.compile("textureset\\s*\\(([\"'](.*?)[\"']|)?\\)");
    private static Pattern texturePattern = Pattern.compile("texture\\s*\\(([\"'](.*?)[\"']|)?\\)");
    private static Pattern[] patterns = new Pattern[] { numPattern, hashPattern, urlPattern,
            vec3Pattern, vec4Pattern, quatPattern, boolPattern, materialPattern, textureSetPattern, texturePattern};


    private static String stripSingleLineComments(String str) {
        str = str.replace("\r", "");
        StringBuffer sb = new StringBuffer();
        String[] lines = str.split("\n");
        for (String line : lines) {
            String lineTrimmed = line.trim();
            // Strip single line comments but preserve "pure" multi-line comments
            // Note that ---[[ is a single line comment
            // You can enable a block in Lua by adding a hyphen, e.g.
            /*
             ---[[
             The block is enabled
             --]]
             */
            if (!lineTrimmed.startsWith("--") || lineTrimmed.startsWith("--[[") || lineTrimmed.startsWith("--]]")) {
                sb.append(line);
            }
            sb.append("\n");
        }
        return sb.toString();
    }

    public static String stripComments(String str) {
        str = stripSingleLineComments(str);
        Matcher matcher = multiLineCommentPattern.matcher(str);

        StringBuffer sb = new StringBuffer();
        while (matcher.find()) {
            // Replace comment with n lines in order to preserve line indices
            int n = matcher.group().split("\n").length;
            StringBuffer lines = new StringBuffer(n);
            for (int i = 0; i < n-1; ++i) lines.append('\n');
            matcher.appendReplacement(sb, lines.toString());
        }
        matcher.appendTail(sb);
        return sb.toString();
    }

    public static List<String> scan(String str) {
        String strStripped = stripComments(str);
        List<Pattern> requirePatterns = Arrays.asList(requirePattern1, requirePattern2, requirePattern3, requirePattern4);

        ArrayList<String> modules = new ArrayList<String>();
        String[] lines = strStripped.split("\n");
        for (String line : lines) {
            line = line.trim();
            // NOTE: At some point we should have a proper lua parser
            for(Pattern requirePattern : requirePatterns) {
                Matcher propMatcher = requirePattern.matcher(line);
                if (propMatcher.matches()) {
                    modules.add(propMatcher.group(1));
                }
            }
        }
        return modules;
    }

    public static class Property {
        public enum Status {
            OK,
            INVALID_ARGS,
            INVALID_VALUE
        }

        /// supported resource property sub-types
        static public long subTypeMaterial = MurmurHash.hash64("material");
        static public long subTypeTextureSet = MurmurHash.hash64("textureset");
        static public long subTypeTexture = MurmurHash.hash64("texture");

        /// Set iff status != INVALID_ARGS
        public String name;
        /// Set iff status == OK
        public PropertyType type;
        /// Set iff status != INVALID_ARGS
        public long subType;
        /// Set iff status != INVALID_ARGS
        public String rawValue;
        /// Set iff status == OK
        public Object value;
        /// Always set
        public int line;
        /// Always set
        public Status status;

        public Property(int line) {
            this.line = line;
        }
    }

    public static String stripProperties(String str) {
        str = str.replace("\r", "");
        StringBuffer sb = new StringBuffer();
        String[] lines = str.split("\n");
        for (String line : lines) {
            Matcher propDeclMatcher = propertyDeclPattern.matcher(line.trim());
            if (!propDeclMatcher.matches()) {
                sb.append(line);
            }
            sb.append("\n");
        }
        return sb.toString();
    }

    public static List<Property> scanProperties(String str) {
        String strStripped = stripComments(str);

        List<Property> properties = new ArrayList<Property>();
        String[] lines = strStripped.split("\n");
        int l = 0; // 0-based line number
        for (String line : lines) {
            line = line.trim();
            Matcher propDeclMatcher = propertyDeclPattern.matcher(line);
            if (propDeclMatcher.matches()) {
                Property property = new Property(l);
                Matcher propArgsMatcher = propertyArgsPattern.matcher(propDeclMatcher.group(1).trim());
                if (!propArgsMatcher.matches()) {
                    property.status = Status.INVALID_ARGS;
                } else {
                    property.name = propArgsMatcher.group(1).trim();
                    property.rawValue = propArgsMatcher.group(2).trim();
                    if (parseProperty(property.rawValue, property)) {
                        property.status = Status.OK;
                    } else {
                        property.status = Status.INVALID_VALUE;
                    }
                }
                properties.add(property);
            }
            ++l;
        }
        return properties;
    }

    private static boolean parseProperty(String rawValue, Property property) {
        boolean result = false;
        for (Pattern pattern : patterns) {
            Matcher matcher = pattern.matcher(property.rawValue);
            if (matcher.matches()) {
                try {
                    if (matcher.pattern() == numPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_NUMBER;
                        property.value = Double.parseDouble(property.rawValue);
                    } else if (matcher.pattern() == hashPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_HASH;
                        property.value = matcher.group(1).trim();
                    } else if (matcher.pattern() == urlPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_URL;
                        if (matcher.group(2) != null) {
                            property.value = matcher.group(2).trim();
                        } else {
                            property.value = "";
                        }
                    } else if (matcher.pattern() == vec3Pattern) {
                        property.type = PropertyType.PROPERTY_TYPE_VECTOR3;
                        Vector3d v = new Vector3d();
                        if (matcher.group(2) != null) {
                            v.set(Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(3)),
                                    Double.parseDouble(matcher.group(4)));
                        }
                        property.value = v;
                    } else if (matcher.pattern() == vec4Pattern) {
                        property.type = PropertyType.PROPERTY_TYPE_VECTOR4;
                        Vector4d v = new Vector4d();
                        if (matcher.group(2) != null) {
                            v.set(Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(3)),
                                    Double.parseDouble(matcher.group(4)),
                                    Double.parseDouble(matcher.group(5)));
                        }
                        property.value = v;
                    } else if (matcher.pattern() == quatPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_QUAT;
                        Quat4d q = new Quat4d();
                        if (matcher.group(2) != null) {
                            q.set(Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(3)),
                                    Double.parseDouble(matcher.group(4)),
                                    Double.parseDouble(matcher.group(5)));
                        }
                        property.value = q;
                    } else if (matcher.pattern() == boolPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_BOOLEAN;
                        property.value = Boolean.parseBoolean(rawValue);
                    } else if (matcher.pattern() == materialPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_RESOURCE;
                        property.subType = Property.subTypeMaterial;
                        property.value = matcher.group(2) == null ? "" :  matcher.group(2).trim();
                    } else if (matcher.pattern() == textureSetPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_RESOURCE;
                        property.subType = Property.subTypeTextureSet;
                        property.value = matcher.group(2) == null ? "" :  matcher.group(2).trim();
                    } else if (matcher.pattern() == texturePattern) {
                        property.type = PropertyType.PROPERTY_TYPE_RESOURCE;
                        property.subType = Property.subTypeTexture;
                        property.value = matcher.group(2) == null ? "" :  matcher.group(2).trim();
                    }
                    result = true;
                } catch (NumberFormatException e) {
                    result = false;
                }
                break;
            }
        }
        return result;
    }

}
