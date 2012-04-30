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

    private static Pattern commentPattern = Pattern.compile("--\\[\\[.*?--\\]\\]|--.*?$",
            Pattern.DOTALL | Pattern.MULTILINE);

    private static Pattern goPattern = Pattern.compile("go\\.property\\s*?\\((.*?)\\)$",
            Pattern.DOTALL | Pattern.MULTILINE);

    private static Pattern identPattern = Pattern.compile("\\w*?[\\\"'](.*?)[\\\"']$",
            Pattern.DOTALL | Pattern.MULTILINE);

    private static Pattern numberPattern = Pattern.compile("[-+]?[0-9]*\\.?[0-9]+");

    private static Pattern urlPattern = Pattern.compile("go\\.url\\s*?\\(\\s*?\\)$");

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
            INVALID,
        }

        private Status status = Status.OK;
        private String error;
        private String name;
        private String value;
        private Type type = Type.INVALID;

        /**
         * Parse property from string, e.g. go.property("my_prop", 123)
         * @param str property to parse
         * @return parsed property. Status is set to {@link Status.PARSE_ERROR} in the strings contains syntax errors
         */
        public static Property parse(String str) {
            StringTokenizer st = new StringTokenizer(str, ",");
            int count = st.countTokens();

            Property property = new Property();

            if (count != 2) {
                return property.setError("Expected two arguments");
            }

            String name = st.nextToken().trim();
            String value = st.nextToken().trim();

            Matcher identMatcher = identPattern.matcher(name);
            if (!identMatcher.matches()) {
                return property.setError("Invalid property name");
            }

            Type type = Type.INVALID;
            if (numberPattern.matcher(value).matches()) {
                type = Type.NUMBER;
            } else if (urlPattern.matcher(value).matches()) {
                type = Type.URL;
            } else {
                return property.setError("Unable to interpret value");
            }

            property.setType(type);
            property.setName(identMatcher.group(1));
            property.setValue(value);

            return property;
        }

        public void setType(Type type) {
            this.type = type;
        }

        public Type getType() {
            return type;
        }

        private void setValue(String value) {
            this.value = value;
        }

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

        public Property setStatus(Status status) {
            this.status  = status;
            return this;
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

    /**
     * Parse lua script for properties
     * @param str string to parse
     * @return array of properites including properties with synax errors
     */
    public static Property[] parse(String str) {
        Matcher matcher = commentPattern.matcher(str);
        String strStripped = matcher.replaceAll("");
        Matcher propMatcher = goPattern.matcher(strStripped);

        List<Property> properties = new ArrayList<LuaPropertyParser.Property>(16);
        while (propMatcher.find()) {
            Property property = Property.parse(propMatcher.group(1));
            properties.add(property);
        }
        return properties.toArray(new Property[properties.size()]);
    }

}
