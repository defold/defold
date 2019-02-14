package com.dynamo.bob.util;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.text.ParseException;
import java.util.Collection;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;

import org.apache.commons.lang3.StringUtils;
import org.apache.commons.io.IOUtils;
import com.dynamo.bob.Bob;

/**
 * .ini-file-link parser abstraction
 * @note Copy-paste of ProjectProperties in order to avoid adding bob as a dependency to most plugins
 * @author chmu
 *
 */
public class BobProjectProperties {
    private Map<String, Map<String, String>> properties;

    /**
     * Constructor with initially empty
     */
    public BobProjectProperties() {
        this.properties = new LinkedHashMap<String, Map<String, String>>();
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
        Map<String, String> group = this.properties.get(category);
        if (group != null) {
            String val = group.get(key);
            if (val != null) {
                return val;
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
        String value = getStringValue(category, key);
        try {
            return new Integer(value);
        } catch (NumberFormatException e) {
            return null;
        }
    }

    /**
     * Get property as integer with default value
     * @param category property category
     * @param key category key
     * @param defaultValue
     * @return property value as integer. defaultValue if not set or on number format errors
     */
    public Integer getIntValue(String category, String key, Integer defaultValue) {
        Map<String, String> group = this.properties.get(category);
        if (group != null) {
            String val = group.get(key);
            if (val != null) {
                try {
                    return new Integer(val);
                } catch (NumberFormatException e) {
                    return defaultValue;
                }
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
        String value = getStringValue(category, key);
        if (value != null) {
            return value.equals("1");
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
        String value = getStringValue(category, key);
        if (value != null) {
            return value.equals("1");
        }
        return defaultValue;
    }

    /**
     * Put property (string variant)
     * @param category property category
     * @param key category key
     * @param value value. if value is null the key/value-pair is effectively removed
     */
    public void putStringValue(String category, String key, String value) {
        Map<String, String> group = this.properties.get(category);
        if (group == null) {
            if (value == null) {
                // Category doesn't exists and the value is null
                // Just return
                return;
            }
            group = new LinkedHashMap<String, String>();
            this.properties.put(category, group);
        }
        if (value != null) {
            group.put(key, value);
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
     * Remove a property
     * @param category property category
     * @param key category key
     */
    public void remove(String category, String key) {
        putStringValue(category, key, null);
    }

    // Helper class to pass tuple of Key+Value
    // via doLoad and a KeyValueFilter class.
    private class StringKeyValue {
        public String key;
        public String value;
        StringKeyValue(String key, String value) {
            this.key = key;
            this.value = value;
        }
    }

    // Helper interface to create a filter for doLoad
    // Used when loading default properties to filter
    // out entries that has valid default values.
    @FunctionalInterface
    private interface KeyValueFilter {
        public boolean filter(StringKeyValue entry);
    }

    private void doLoad(InputStream in, KeyValueFilter passFunc) throws IOException, ParseException {
        BufferedReader reader = new BufferedReader(new InputStreamReader(in));
        Map<String, String> propGroup = null;
        int cursor = 0;
        String line = reader.readLine();
        while (line != null) {
            line.trim();
            if (line.startsWith("[")) {
                if (!line.endsWith("]")) {
                    throw new ParseException("invalid category: " + line, cursor);
                }
                String category = line.substring(1, line.length() - 1);
                propGroup = this.properties.get(category);
                if (propGroup == null) {
                    propGroup = new LinkedHashMap<String, String>();
                    this.properties.put(category, propGroup);
                }
            } else if (line.length() > 0) {
                if (propGroup == null) {
                    throw new ParseException("no initial category", 0);
                }
                int sep = line.indexOf('=');
                boolean valid = true;
                if (sep != -1) {
                    try {
                        String key = line.substring(0, sep).trim();
                        String value = line.substring(sep + 1).trim();
                        if (passFunc != null) {
                            StringKeyValue keyVal = new StringKeyValue(key, value);
                            if (passFunc.filter(keyVal)) {
                                propGroup.put(keyVal.key, value);
                            }
                        } else {
                            propGroup.put(key, value);
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
    }

    /**
     * Load default properties from the internal meta.properties file
     * @throws IOException
     * @throws ParseException
     */
    public void loadDefaults() throws IOException, ParseException {
        KeyValueFilter filterDefaults = new KeyValueFilter(){
           @Override
           public boolean filter(StringKeyValue entry) {
                // Only keep entries where key ends with ".default"
                if (entry.key.endsWith(".default") && !entry.value.isEmpty()) {

                    // Modify key to remove ".default" part
                    entry.key = entry.key.substring(0, entry.key.length() - ".default".length());
                    return true;
                }

                return false;
            }
        };

        InputStream is = Bob.class.getResourceAsStream("meta.properties");
        try {
            doLoad(is, filterDefaults);
        } catch (ParseException e) {
            throw new RuntimeException("Failed to parse meta.properties", e);
        } finally {
            IOUtils.closeQuietly(is);
        }
    }

    /**
     * Load properties in-place from {@link InputStream}
     * @param in {@link InputStream} to load from
     * @throws IOException
     * @throws ParseException
     */
    public void load(InputStream in) throws IOException, ParseException {
        try {
            doLoad(in, null);
        } finally {
            IOUtils.closeQuietly(in);
        }
    }

    /**
     * Get all category names
     * @return {@link Collection} of cateory names
     */
    public Collection<String> getCategoryNames() {
        return properties.keySet();
    }

    /**
     * Save to PrintWriter
     * @param pw {@link PrintWriter} to save to
     */
    public void save(PrintWriter pw) {
        for (String category : getCategoryNames()) {
            pw.format("[%s]%n", category);

            for (String key : getKeys(category)) {
                String value = getStringValue(category, key);
                pw.format("%s = %s%n", key, value);
            }
            pw.println();
        }
        pw.close();
    }

    /**
     * Save to OutputStream
     * @param os {@link OutputStream} to save to
     * @throws IOException
     */
    public void save(OutputStream os) throws IOException {
        PrintWriter pw = new PrintWriter(os);
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

    /**
     * Get all keys for given category
     * @param category category to get keys for
     * @return collection of keys
     */
    public Collection<String> getKeys(String category) {
        Map<String, String> group = this.properties.get(category);
        if (group != null) {
            return group.keySet();
        } else {
            return Collections.emptySet();
        }
    }

}
