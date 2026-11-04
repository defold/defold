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

package com.dynamo.bob.util;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.charset.StandardCharsets;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.lang.reflect.Field;

import org.apache.commons.lang3.ArrayUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.Bob;

/**
 * .ini-file-link parser abstraction
 *
 */
public class BobProjectProperties {

    public final static String PROPERTIES_PROJECT_FILE = "game.properties";
    public final static String PROPERTIES_FILE = "ext.properties";
    public final static String PROPERTIES_INTERNAL_FILE = "meta.properties";

    public enum PropertyType {
        BOOL("bool"),
        STRING("string"),
        NUMBER("number"),
        INTEGER("integer"),
        STRING_ARRAY("string_array"),
        RESOURCE("resource");

        private String type;

        PropertyType(String propType) {
            this.type = type;
        }

        public String getType() {
            return this.type;
        }
    }

    // A single "key += value" or "key -= value" entry from a settings file
    private static class ArrayOperation {
        final boolean isAdd;
        final String value;

        ArrayOperation(boolean isAdd, String value) {
            this.isAdd = isAdd;
            this.value = value;
        }
    }

    private static class ProjectProperty {
        private String value;
        private String defaultValue;
        private final List<String> defaultValues = new ArrayList<String>();
        PropertyType type;
        private Boolean isPrivate;

        private Map<Integer, String> valuesArray;

        // "+=" and "-=" entries, applied when the file is merged on top of earlier ones
        private List<ArrayOperation> arrayOperations;

        public ProjectProperty() {
        }

        public String toString() {
            return getValue();
        }

        public void setAttribute(String key, String value) {
            if (key == null || key.isEmpty() || value == null) {
                return;
            }
            switch(key) {
                case "value":
                    this.value = value;
                    break;
                case "type":
                    this.type = PropertyType.valueOf(StringUtil.toUpperCase(value));
                    break;
                case "default":
                    this.defaultValue = value;
                    this.defaultValues.add(value);
                    break;
                case "help":
                    // no need in bob
                    break;
                case "private":
                    this.isPrivate = value.equals("1");
                    break;
            }
        }

        public Object getTypedValue(PropertyType type) {
            try {
                String val = getValue();
                Object result = null;
                if (val == null) {
                    return null;
                }
                switch(type) {
                    case STRING:
                    case RESOURCE:
                        result = val;
                        break;
                    case INTEGER:
                        result = Integer.parseInt(val);
                        break;
                    case NUMBER:
                        result = Float.parseFloat(val);
                        break;
                    case BOOL:
                        result = val.equals("1");
                        break;
                    case STRING_ARRAY:
                        if (this.valuesArray == null) {
                            parseValueAsValuesArray();
                        }
                        result = this.valuesArray;
                        break;
                }
                return result;

            } catch (Exception e) {
                throw new RuntimeException("Failed to get typed value for property", e);
            }
        }

        public Object getTypedValue() {
            return getTypedValue(this.type);
        }

        public void overrideBy(ProjectProperty prop) {
            this.defaultValues.addAll(prop.defaultValues);
            try {
                Field[] fields = ProjectProperty.class.getDeclaredFields();
                for(Field f : fields) {
                    Class t = f.getType();
                    Object v = f.get(prop);
                    if ((v != null) && (t.isEnum() || t.equals(String.class) || t.equals(Boolean.class))) {
                        f.set(this, v);
                        if (f.getName() == "value") {
                            // it's important to reset old parsed values from value Array if value is overwritten
                            valuesArray = null;
                        }
                    }
                }
            }
            catch (IllegalArgumentException e) {
                throw new RuntimeException("Can't override field ", e);
            }
            catch (IllegalAccessException e) {
                throw new RuntimeException("Can't access field ", e);
            }
        }

        public String getValue() {
            return this.value != null ? this.value : this.defaultValue;
        }
        
        public PropertyType getType() {
            return this.type;
        }

        public void addArrayValue(String index, String value) {
            if (this.valuesArray == null) {
                this.valuesArray = new TreeMap<Integer, String>();
            }
            if (index == null || index.isEmpty() || value == null) {
                return;
            }
            try {
                int indexNum = Integer.parseInt(index);
                this.valuesArray.put(indexNum, value);
                this.value = String.join(",", this.valuesArray.values());
            }
            catch (Exception e) {
                throw new RuntimeException("Can't add element from array property", e);
            }

        }

        public void addArrayOperation(boolean isAdd, String value) {
            if (this.arrayOperations == null) {
                this.arrayOperations = new ArrayList<ArrayOperation>();
            }
            for (String v : parseStringArray(value)) {
                this.arrayOperations.add(new ArrayOperation(isAdd, v));
            }
        }

        public boolean hasArrayOperations() {
            return this.arrayOperations != null;
        }

        // Applies "+=" and "-=" entries on top of the current list value
        public void applyArrayOperations(List<ArrayOperation> operations, String category, String key) throws ParseException {
            if (this.type == PropertyType.BOOL || this.type == PropertyType.INTEGER || this.type == PropertyType.NUMBER || this.type == PropertyType.RESOURCE) {
                throw new ParseException(String.format("'%s.%s' is not a list, += and -= can't be used", category, key), 0);
            }
            List<String> values = parseStringArray(getValue());
            for (ArrayOperation op : operations) {
                if (op.isAdd) {
                    if (!values.contains(op.value)) {
                        values.add(op.value);
                    }
                }
                else if (!values.removeIf(v -> v.equals(op.value))) {
                    throw new ParseException(String.format("'%s.%s' -= %s: value not found in the list", category, key, op.value), 0);
                }
            }
            this.value = String.join(",", values);
            this.valuesArray = null;
        }

        public boolean isDefault() {
            return this.value == null && this.defaultValue != null;
        }

        public Boolean isPrivate() {
            return this.isPrivate != null && this.isPrivate;
        }

        // parse string as comma separater list of strings
        private void parseValueAsValuesArray() {
            Map<Integer, String> outValues = new TreeMap<Integer, String>();
            int counter = 0;
            for (String v : parseStringArray(this.value)) {
                outValues.put(counter, v);
                counter++;
            }
            this.valuesArray = outValues;
        }

    }

    // Splits a comma separated value into trimmed, non-empty entries
    private static List<String> parseStringArray(String rawValue) {
        List<String> values = new ArrayList<String>();
        if (rawValue == null) {
            return values;
        }
        for (String value : rawValue.split(",")) {
            value = value.trim();
            if (!value.isEmpty()) {
                values.add(value);
            }
        }
        return values;
    }

    private Map<String, Map<String, ProjectProperty>> properties;

    /**
     * Constructor with initially empty
     */
    public BobProjectProperties() {
        this.properties = new LinkedHashMap<String, Map<String, ProjectProperty>>();
    }

    public Map<String, Map<String, Object>> createTypedMap(PropertyType[] types) {
        Map<String, Map<String, Object>> result = new HashMap<String, Map<String, Object>>();
        for (String categoryName : properties.keySet()) {
            Map<String, Object> resultCategory = new HashMap<String, Object>();
            result.put(categoryName, resultCategory);
            Map<String, ProjectProperty> category = properties.get(categoryName);
            for (String key : category.keySet()) {
                ProjectProperty prop = category.get(key);
                if (ArrayUtils.contains(types, prop.getType())) {
                    resultCategory.put(key, prop.getTypedValue());
                }
                else {
                    resultCategory.put(key, prop.getValue());
                }
                
            }
        }
        return result;
    }

    /**
     * Load properties in-place from {@link InputStream}
     * @param in {@link InputStream} to load from
     * @param isMeta if loads meta file or not
     * @throws IOException
     * @throws ParseException
     */
    public void load(InputStream in, boolean isMeta) throws IOException, ParseException {
        try {
            Map<String, Map<String, ProjectProperty>> props = doLoad(in, isMeta);
            // merge into properties
            BobProjectProperties.mergeProperties(properties, props);
        } finally {
            IOUtils.closeQuietly(in);
        }
    }

    /**
     * Load properties in-place from {@link InputStream} as non-meta file
     * @param in {@link InputStream} to load from
     * @throws IOException
     * @throws ParseException
     */
    public void load(InputStream in) throws IOException, ParseException {
        load(in, false);
    }

    public void cleanupEmptyProperties() {
        properties.remove("");
    }

    /**
     * Load default properties from the internal meta.properties file
     * @throws IOException
     * @throws ParseException
     */
    public void loadDefaultMetaFile()  throws IOException, ParseException {
        InputStream is = Bob.class.getResourceAsStream(PROPERTIES_INTERNAL_FILE);
        try {
            load(is, true);
        } catch (ParseException e) {
            throw new RuntimeException("Failed to parse meta.properties", e);
        } finally {
            IOUtils.closeQuietly(is);
        }
    }

    /**
     * Get property as an array of strings based on a comma separated value, with default value
     * @param category property category
     * @param key category key
     * @param defaultValue
     * @return property value as an array of strings. defaultValue if not set
     */
    public String[] getStringArrayValue(String category, String key, String[] defaultValue) {
        Map<String, ProjectProperty> group = this.properties.get(category);
        if (group != null) {
            ProjectProperty val = group.get(key);
            if (val != null) {
                if (val.valuesArray == null) {
                    if (val.value == null) {
                        return defaultValue;
                    }
                    val.parseValueAsValuesArray();
                }
                return val.valuesArray.values().toArray(new String[0]);
            }
        }
        return defaultValue;
    }

    /**
     * Get property as an array of strings based on a comma separated value
     * @param category property category
     * @param key category key
     * @return property value as an array of strings. defaultValue if not set
     */
    public String[] getStringArrayValue(String category, String key) {
        return getStringArrayValue(category, key, new String[0]);
    }

    private static void addStringArrayValues(LinkedHashSet<String> values, String rawValue) {
        values.addAll(parseStringArray(rawValue));
    }

    /**
     * Get property as an array of strings, merging all contributed default values and the explicit value.
     * This is intended for comma-separated string settings that may be contributed by meta property files
     * and extended by the project's game.project.
     * @param category property category
     * @param key category key
     * @param defaultValue returned if neither the default nor explicit value has any entries
     * @return merged property values with duplicates removed while preserving order
     */
    public String[] getStringArrayValueMerged(String category, String key, String[] defaultValue) {
        ProjectProperty val = getValue(category, key);
        if (val == null) {
            return defaultValue;
        }

        LinkedHashSet<String> values = new LinkedHashSet<String>();
        for (String contributedDefaultValue : val.defaultValues) {
            addStringArrayValues(values, contributedDefaultValue);
        }
        addStringArrayValues(values, val.value);

        if (values.isEmpty()) {
            return defaultValue;
        }

        List<String> merged = new ArrayList<String>(values);
        return merged.toArray(new String[0]);
    }

    /**
     * Get property as string
     * @param category property category
     * @param key category key
     * @return property value. null if not set
     */
    public String getStringValue(String category, String key) {
        return getStringValue(category, key, null);
    }

    /**
     * Get property as string with default value
     * @param category
     * @param key
     * @param defaultValue
     * @return property value. defaultValue if not set
     */
    public String getStringValue(String category, String key, String defaultValue) {
        ProjectProperty val = getValue(category, key);
        if (val != null) {
            String result = val.getValue();
            if (result != null) {
                return result;
            }
        }
        return defaultValue;
    }

    /**
     * Get property as boolean
     * @param category property category
     * @param key category key
     * @return property value as boolean. null if not set.
     */
    public Boolean getBooleanValue(String category, String key) {
        ProjectProperty val = getValue(category, key);
        if (val != null) {
            return (Boolean)val.getTypedValue(BobProjectProperties.PropertyType.BOOL);
        }
        return null;
    }

    /**
     * Get property as boolean with default
     * @param category property category
     * @param key category key
     * @return property value as boolean. defaultValue if not set
     */
    public Boolean getBooleanValue(String category, String key, Boolean defaultValue) {
        ProjectProperty val = getValue(category, key);
        if (val != null) {
            Boolean result = (Boolean)val.getTypedValue(BobProjectProperties.PropertyType.BOOL);
            if (result != null) {
                return result;
            }
        }
        return defaultValue;
    }

    /**
     * Get property as integer
     * @param category property category
     * @param key category key
     * @return property value as integer. null if not set or on number format errors
     */
    public Integer getIntValue(String category, String key) {
        ProjectProperty val = getValue(category, key);
        if (val != null) {
            return (Integer)val.getTypedValue(BobProjectProperties.PropertyType.INTEGER);
        }
        return null;
    }

    /**
     * Get property as integer with default value
     * @param category property category
     * @param key category key
     * @param defaultValue
     * @return property value as integer. defaultValue if not set or on number format errors
     */
    public Integer getIntValue(String category, String key, Integer defaultValue) {
       ProjectProperty val = getValue(category, key);
        if (val != null) {
            Integer result = (Integer)val.getTypedValue(BobProjectProperties.PropertyType.INTEGER);
            if (result != null) {
                return result;
            }
        }
        return defaultValue;
    }

    /**
     * Check if the user didn't set this value
     * @param category property category
     * @param key category key
     * @return return true if default value;
     */
    public Boolean isDefault(String category, String key) {
        Map<String, ProjectProperty> group = this.properties.get(category);
        if (group != null && group.containsKey(key)) {
            return group.get(key).isDefault();
        }
        return false;
    }

    /**
     * Check if value is private
     * @param category property category
     * @param key category key
     * @return return true if private value;
     */
    public Boolean isPrivate(String category, String key) {
        Map<String, ProjectProperty> group = this.properties.get(category);
        if (group != null && group.containsKey(key)) {
            return group.get(key).isPrivate();
        }
        return false;
    }

    /**
     * Put property (string variant)
     * @param category property category
     * @param key category key
     * @param value value. if value is null the key/value-pair is effectively removed
     */
    public void putStringValue(String category, String key, String value) {
        Map<String, ProjectProperty> group = this.properties.get(category);
        if (group == null) {
            if (value == null) {
                // Category doesn't exists and the value is null
                // Just return
                return;
            }
            group = new LinkedHashMap<String, ProjectProperty>();
            this.properties.put(category, group);
        }
        if (value != null) {
            ProjectProperty prop = new ProjectProperty();
            prop.setAttribute("value", value);
            group.put(key, prop);
        } else {
            group.remove(key);
        }
    }

    /**
     * Put property (int variant)
     * @param category property category
     * @param key category key
     * @param value value
     */
    public void putIntValue(String category, String key, int value) {
        putStringValue(category, key, Integer.toString(value));
    }

    /**
     * Put property (boolean variant)
     * @param category property category
     * @param key category key
     * @param value value
     */
    public void putBooleanValue(String category, String key, boolean value) {
        putStringValue(category, key, value ? "1" : "0");
    }

    /**
     * Remove all private fields
     */
    public void removePrivateFields() {
        for (String categoryName : properties.keySet()) {
            Map<String, ProjectProperty> category = properties.get(categoryName);
            category.entrySet().removeIf(entry -> entry.getValue().isPrivate());
        }
    }

    private ProjectProperty getValue(String category, String key) {
        Map<String, ProjectProperty> group = this.properties.get(category);
        if (group != null) {
            ProjectProperty val = group.get(key);
            return val;
        }
        return null;
    }

    // Merge properties from b to a
    private static void mergeProperties(Map<String, Map<String, ProjectProperty>> a, Map<String, Map<String, ProjectProperty>> b) throws ParseException {
        for (String category : b.keySet()) {
            if (!a.containsKey(category)) {
                a.put(category, new LinkedHashMap<String, ProjectProperty>());
            }
            Map<String, ProjectProperty> propGroupA = a.get(category);
            Map<String, ProjectProperty> propGroupB = b.get(category);
            for (String key : propGroupB.keySet()) {
                ProjectProperty propB = propGroupB.get(key);
                if (!propGroupA.containsKey(key)) {
                    if (!propB.hasArrayOperations()) {
                        propGroupA.put(key, propB);
                        continue;
                    }
                    // "+=" and "-=" on a list that isn't set yet start from an empty list
                    propGroupA.put(key, new ProjectProperty());
                }
                ProjectProperty propA = propGroupA.get(key);
                propA.overrideBy(propB);
                if (propB.hasArrayOperations()) {
                    propA.applyArrayOperations(propB.arrayOperations, category, key);
                }
            }
        }
    }

    private Map<String, Map<String, ProjectProperty>> doLoad(InputStream in, boolean isMeta) throws IOException, ParseException {
        Map<String, Map<String, ProjectProperty>> properties = new LinkedHashMap<String, Map<String, ProjectProperty>>();
        BufferedReader reader = new BufferedReader(new InputStreamReader(in));
        Map<String, ProjectProperty> propGroup = null;
        int cursor = 0;
        String line = reader.readLine();
        while (line != null) {
            line = line.trim();
            if (line.startsWith("[")) {
                if (!line.endsWith("]")) {
                    throw new ParseException("invalid category: " + line, cursor);
                }
                String category = line.substring(1, line.length() - 1);
                propGroup = properties.get(category);
                if (propGroup == null) {
                    propGroup = new LinkedHashMap<String, ProjectProperty>();
                    properties.put(category, propGroup);
                }
            } else if (line.length() > 0) {
                if (propGroup == null) {
                    throw new ParseException("no initial category", 0);
                }
                int sep = line.indexOf('=');
                boolean valid = true;
                if (sep != -1) {
                    try {
                        String fullKey = line.substring(0, sep).trim();
                        String value = line.substring(sep + 1).trim();
                        char operator = sep > 0 ? line.charAt(sep - 1) : 0;
                        if (!isMeta && (operator == '+' || operator == '-')) {
                            // "key += value" adds to a list, "key -= value" removes from it
                            String key = line.substring(0, sep - 1).trim();
                            if (key.isEmpty() || key.contains(".") || key.contains("#") || key.endsWith("+") || key.endsWith("-")) {
                                throw new ParseException("invalid key for " + operator + "=: " + line, cursor);
                            }
                            if (parseStringArray(value).isEmpty()) {
                                throw new ParseException("missing value for " + operator + "=: " + line, cursor);
                            }
                            ProjectProperty prop = propGroup.get(key);
                            if (prop == null) {
                                prop = new ProjectProperty();
                            }
                            prop.addArrayOperation(operator == '+', value);
                            propGroup.put(key, prop);
                        }
                        else if (!isMeta && (fullKey.endsWith("+") || fullKey.endsWith("-"))) {
                            throw new ParseException("invalid +=/-=, no space allowed before '=': " + line, cursor);
                        }
                        else if(fullKey.contains(".")) {
                            String[] keyAndAttr = fullKey.split("\\.");
                            String key = keyAndAttr[0];
                            ProjectProperty prop = propGroup.get(key);
                            if (prop == null) {
                                prop = new ProjectProperty();
                            }
                            prop.setAttribute(keyAndAttr[1], value);
                            propGroup.put(key, prop);
                        }
                        else if (!isMeta && fullKey.contains("#")) {
                            String[] keyAndIndex = fullKey.split("#");
                            String key = keyAndIndex[0];
                            ProjectProperty prop = propGroup.get(key);
                            if (prop == null) {
                                prop = new ProjectProperty();
                            }
                            prop.addArrayValue(keyAndIndex[1], value);
                            propGroup.put(key, prop);
                        }
                        else if (!isMeta)
                        {
                            String key = fullKey;
                            ProjectProperty prop = propGroup.get(key);
                            if (prop == null) {
                                prop = new ProjectProperty();
                            }
                            prop.setAttribute("value", value);
                            propGroup.put(key, prop); 
                        }
                    } catch (IndexOutOfBoundsException e) {
                        valid = false;
                    }
                } else {
                    valid = false;
                }
                if (!valid) {
                    throw new ParseException("invalid key/value-pair: " + line, cursor);
                }
            }
            cursor += line.length();
            line = reader.readLine();
        }
        for (Map.Entry<String, Map<String, ProjectProperty>> group : properties.entrySet()) {
            for (Map.Entry<String, ProjectProperty> entry : group.getValue().entrySet()) {
                ProjectProperty prop = entry.getValue();
                if (prop.hasArrayOperations() && prop.value != null) {
                    throw new ParseException(String.format("'%s.%s' is both set and changed with +=/-= in the same file", group.getKey(), entry.getKey()), 0);
                }
            }
        }
        return properties;
    }

    /**
     * Get all category names
     * @return {@link Collection} of cateory names
     */
    public Collection<String> getCategoryNames() {
        return properties.keySet();
    }

    /**
     * Get all keys for given category
     * @param category category to get keys for
     * @return collection of keys
     */
    public Collection<String> getKeys(String category) {
        Map<String, ProjectProperty> group = this.properties.get(category);
        if (group != null) {
            return group.keySet();
        } else {
            return Collections.emptySet();
        }
    }

    /**
     * Save to PrintWriter
     * @param pw {@link PrintWriter} to save to
     */
    public void save(PrintWriter pw) {
        // Line endings are hardcoded to '\n' rather than the platform separator: this ends up
        // in game.projectc, whose size the HTML5 loader verifies. See issue #10006.
        for (String category : getCategoryNames()) {
            pw.format("[%s]\n", category);

            for (String key : getKeys(category)) {
                ProjectProperty prop = getValue(category, key);
                String value = prop.getValue();
                if (value != null) {
                    pw.format("%s = %s\n", key, value);
                }
            }
            pw.print("\n");
        }
        pw.close();
    }

    /**
     * Save to OutputStream
     * @param os {@link OutputStream} to save to
     * @throws IOException
     */
    public void save(OutputStream os) throws IOException {
        PrintWriter pw = new PrintWriter(new OutputStreamWriter(os, StandardCharsets.UTF_8));
        save(pw);
        os.close();
    }

    /**
     * Serialize properties to String
     * @return properties serialized as a String
     */
    public String serialize() {
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        save(pw);
        return sw.toString();
    }

}
