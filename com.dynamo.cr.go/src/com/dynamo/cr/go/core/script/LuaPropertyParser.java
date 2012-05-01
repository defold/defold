package com.dynamo.cr.go.core.script;

import java.util.ArrayList;
import java.util.List;
import java.util.StringTokenizer;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Simple parser for script properties
 * @author chmu
 *
 */
public class LuaPropertyParser {

    private static Pattern multiLineCommentPattern = Pattern.compile("--\\[\\[.*?--\\]\\]",
            Pattern.DOTALL | Pattern.MULTILINE);

    private static Pattern goPattern = Pattern.compile("go\\.property\\s*?\\((.*?)\\)$",
            Pattern.DOTALL | Pattern.MULTILINE);

    private static Pattern identPattern = Pattern.compile("\\w*?[\\\"'](.*?)[\\\"']$",
            Pattern.DOTALL | Pattern.MULTILINE);

    private static Pattern numberPattern = Pattern.compile("[-+]?[0-9]*\\.?[0-9]+");

    private static Pattern urlPattern = Pattern.compile("go\\.url\\s*?\\(\\s*?\\)$");

    private static Pattern hashPattern = Pattern.compile("hash\\s*?\\(\\s*?[\\\"'](.*?)[\\\"']\\s*?\\)$");

    /**
     * Script property representation
     * @author chmu
     *
     */
    public static class Property {

        /**
         * Property parse status
         *
         */
        public static enum Status {
            OK,
            PARSE_ERROR,
        }

        /**
         * Inferred property type
         *
         */
        public static enum Type {
            NUMBER,
            URL,
            HASH,
            INVALID,
        }

        private Status status = Status.OK;
        private String error;
        private String name;
        private String value;
        private Type type = Type.INVALID;
        private int line;

        /**
         * Parse property from string, e.g. go.property("my_prop", 123)
         * @param line
         * @param str property to parse
         * @return parsed property. Status is set to {@link Status.PARSE_ERROR} in the strings contains syntax errors
         */
        public static Property parse(int line, String str) {
            StringTokenizer st = new StringTokenizer(str, ",");
            int count = st.countTokens();

            Property property = new Property();
            property.setLine(line);

            if (count != 2) {
                return property.setError("Expected two arguments");
            }

            String name = st.nextToken().trim();
            String valueRaw = st.nextToken().trim();
            String value = "";

            Matcher identMatcher = identPattern.matcher(name);
            if (!identMatcher.matches()) {
                return property.setError("Invalid property name");
            }

            Matcher m;
            Type type = Type.INVALID;
            if (numberPattern.matcher(valueRaw).matches()) {
                type = Type.NUMBER;
                value = valueRaw;
            } else if (urlPattern.matcher(valueRaw).matches()) {
                type = Type.URL;
            } else if ((m = hashPattern.matcher(valueRaw)).matches()) {
                type = Type.HASH;
                value = m.group(1);
            } else {
                return property.setError("Unable to interpret value");
            }

            property.setType(type);
            property.setName(identMatcher.group(1));
            property.setValue(value);

            return property;
        }

        private void setLine(int line) {
            this.line = line;
        }

        public int getLine() {
            return line;
        }

        private void setType(Type type) {
            this.type = type;
        }

        public Type getType() {
            return type;
        }

        private void setValue(String value) {
            this.value = value;
        }

        /**
         * Get the argument value. For number the number value as string is returned,
         * for hash() for string to hash is returned. For functions with empty argument, eg go.url(),
         * the empty string is returned
         * @return the value as string
         */
        public String getValue() {
            return value;
        }

        private void setName(String name) {
            this.name = name;
        }

        public String getName() {
            return name;
        }

        private Property setError(String error) {
            this.error = error;
            this.status = Status.PARSE_ERROR;
            return this;
        }

        public String getError() {
            return error;
        }

        public Status getStatus() {
            return status;
        }

        @Override
        public String toString() {
            if (status == Status.OK) {
                return String.format("%s = %s", name, value);
            } else {
                return this.error;
            }
        }
    }

    private static String stripSingleLineComments(String str) {
        str = str.replace("\r", "");
        StringBuffer sb = new StringBuffer();
        String[] lines = str.split("\n");
        for (String line : lines) {
            String lineTrimmed = line.trim();
            // Strip single line comments but preserve "pure" multiline comments
            // Note that ---[[ is a single line comment
            // You can enbable a block in Lua by adding a hypen, eg
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


    /**
     * Parse lua script for properties
     * @param str string to parse
     * @return array of properites including properties with synax errors
     */
    public static Property[] parse(String str) {
        str = stripSingleLineComments(str);
        Matcher matcher = multiLineCommentPattern.matcher(str);

        StringBuffer sb = new StringBuffer();
        while (matcher.find()) {
            // Replace comment with n lines in order to prevserve line indices
            int n = matcher.group().split("\n").length;
            StringBuffer lines = new StringBuffer(n);
            for (int i = 0; i < n; ++i) lines.append('\n');
            matcher.appendReplacement(sb, lines.toString());
        }
        matcher.appendTail(sb);
        String strStripped = sb.toString();

        List<Property> properties = new ArrayList<LuaPropertyParser.Property>(16);

        int i = 0;
        String[] lines = strStripped.split("\n");
        for (String line : lines) {
            line = line.trim();
            Matcher propMatcher = goPattern.matcher(line);
            while (propMatcher.find()) {
                Property property = Property.parse(i, propMatcher.group(1));
                properties.add(property);
            }
            ++i;
        }
        return properties.toArray(new Property[properties.size()]);
    }

}
