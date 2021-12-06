// Copyright 2020 The Defold Foundation
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
import com.dynamo.bob.pipeline.DefoldLuaParser.PropertyAndLine;

public class LuaScanner {

    private static Pattern propertyDeclPattern = Pattern.compile("go.property\\s*?\\((.*?)\\);?(\\s*?--.*?)?$");
    private static Pattern propertyArgsPattern = Pattern.compile("[\"'](.*?)[\"']\\s*,(.*)");

    // http://docs.python.org/dev/library/re.html#simulating-scanf
    private static Pattern numPattern = Pattern.compile("[-+]?(\\d+(\\.\\d*)?|\\.\\d+)([eE][-+]?\\d+)?");
    private static Pattern hashPattern = Pattern.compile("hash\\s*\\([\"'](.*?)[\"']\\)");
    private static Pattern urlPattern = Pattern.compile("msg\\.url\\s*\\(([\"'](.*?)[\"']|)?\\)");
    private static Pattern vec3Pattern1 = Pattern.compile("vmath\\.vector3\\s*\\(((.+?),(.+?),(.+?))\\)");
    private static Pattern vec3Pattern2 = Pattern.compile("vmath\\.vector3\\s*\\(((.+?))\\)");
    private static Pattern vec3Pattern3 = Pattern.compile("vmath\\.vector3\\s*\\(()\\)");
    private static Pattern vec4Pattern1 = Pattern.compile("vmath\\.vector4\\s*\\(((.+?),(.+?),(.+?),(.+?))\\)");
    private static Pattern vec4Pattern2 = Pattern.compile("vmath\\.vector4\\s*\\(((.+?))\\)");
    private static Pattern vec4Pattern3 = Pattern.compile("vmath\\.vector4\\s*\\(()\\)");
    private static Pattern quatPattern = Pattern.compile("vmath\\.quat\\s*\\(((.*?),(.*?),(.*?),(.*?)|)\\)");
    private static Pattern boolPattern = Pattern.compile("(false|true)");
    private static Pattern resourcePattern = Pattern.compile("resource\\.(.*?)\\s*\\(([\"'](.*?)[\"']|)?\\)");
    private static Pattern[] patterns = new Pattern[] { numPattern, hashPattern, urlPattern,
            vec3Pattern1, vec3Pattern2, vec3Pattern3, vec4Pattern1, vec4Pattern2, vec4Pattern3, quatPattern, boolPattern, resourcePattern};


    public static List<String> scan(String str) {
        DefoldLuaParser luaParser = new DefoldLuaParser();
        luaParser.setStripComments(true);
        String out = luaParser.parse(str);
        return luaParser.getModules();
    }

    public static class Property {
        public enum Status {
            OK,
            INVALID_ARGS,
            INVALID_VALUE
        }

        /// Set iff status != INVALID_ARGS
        public String name;
        /// Set iff status == OK
        public PropertyType type;
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
        DefoldLuaParser luaParser = new DefoldLuaParser();
        luaParser.setStripComments(true);
        luaParser.setStripProperties(true);
        return luaParser.parse(str);
    }

    public static List<Property> scanProperties(String str) {
        DefoldLuaParser luaParser = new DefoldLuaParser();
        luaParser.setStripComments(true);
        luaParser.parse(str);

        List<Property> properties = new ArrayList<Property>();
        for(PropertyAndLine p : luaParser.getProperties()) {
            Property property = parseProperty(p.property, p.line);
            if (property != null) {
                properties.add(property);
            }
        }
        return properties;
    }


    private static Property parseProperty(String propString, int line) {
        Matcher propDeclMatcher = propertyDeclPattern.matcher(propString);
        if (!propDeclMatcher.matches()) {
            return null;
        }
        Property property = new Property(line);
        Matcher propArgsMatcher = propertyArgsPattern.matcher(propDeclMatcher.group(1).trim());
        if (!propArgsMatcher.matches()) {
            property.status = Status.INVALID_ARGS;
        } else {
            property.name = propArgsMatcher.group(1).trim();
            property.rawValue = propArgsMatcher.group(2).trim();
            if (parsePropertyValue(property.rawValue, property)) {
                property.status = Status.OK;
            } else {
                property.status = Status.INVALID_VALUE;
            }
        }

        return property;
    }


    private static boolean parsePropertyValue(String rawValue, Property property) {
        boolean result = false;
        for (Pattern pattern : patterns) {
            Matcher matcher = pattern.matcher(property.rawValue);
            if (matcher.matches()) {
                try {
                    final Pattern matchedPattern = matcher.pattern();
                    if (matchedPattern == numPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_NUMBER;
                        property.value = Double.parseDouble(property.rawValue);
                    } else if (matchedPattern == hashPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_HASH;
                        property.value = matcher.group(1).trim();
                    } else if (matchedPattern == urlPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_URL;
                        if (matcher.group(2) != null) {
                            property.value = matcher.group(2).trim();
                        } else {
                            property.value = "";
                        }
                    } else if ((matchedPattern == vec3Pattern1) || (matchedPattern == vec3Pattern2) || (matchedPattern == vec3Pattern3)) {
                        property.type = PropertyType.PROPERTY_TYPE_VECTOR3;
                        Vector3d v = new Vector3d();
                        if (matchedPattern == vec3Pattern1) {
                            v.set(Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(3)),
                                    Double.parseDouble(matcher.group(4)));
                        }
                        else if (matchedPattern == vec3Pattern2) {
                            v.set(Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(2)));
                        }
                        property.value = v;
                    } else if ((matchedPattern == vec4Pattern1) || (matchedPattern == vec4Pattern2) || (matchedPattern == vec4Pattern3)) {
                        property.type = PropertyType.PROPERTY_TYPE_VECTOR4;
                        Vector4d v = new Vector4d();
                        if (matchedPattern == vec4Pattern1) {
                            v.set(Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(3)),
                                    Double.parseDouble(matcher.group(4)),
                                    Double.parseDouble(matcher.group(5)));
                        }
                        else if (matchedPattern == vec4Pattern2) {
                            v.set(Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(2)));
                        }
                        property.value = v;
                    } else if (matchedPattern == quatPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_QUAT;
                        Quat4d q = new Quat4d();
                        if (matcher.group(2) != null) {
                            q.set(Double.parseDouble(matcher.group(2)),
                                    Double.parseDouble(matcher.group(3)),
                                    Double.parseDouble(matcher.group(4)),
                                    Double.parseDouble(matcher.group(5)));
                        }
                        property.value = q;
                    } else if (matchedPattern == boolPattern) {
                        property.type = PropertyType.PROPERTY_TYPE_BOOLEAN;
                        property.value = Boolean.parseBoolean(rawValue);
                    } else if (matchedPattern == resourcePattern) {
                        property.type = PropertyType.PROPERTY_TYPE_HASH;
                        property.value = matcher.group(3) == null ? "" :  matcher.group(3).trim();
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
