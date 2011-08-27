package com.dynamo.cr.editor.core;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.text.ParseException;
import java.util.HashMap;
import java.util.Map;

public class ProjectProperties {
    Map<String, Map<String, String>> properties;

    public ProjectProperties() {
        this.properties = new HashMap<String, Map<String, String>>();
    }

    public String getStringValue(String category, String key) {
        Map<String, String> group = this.properties.get(category);
        if (group != null) {
            return group.get(key);
        }
        return null;
    }

    public Integer getIntValue(String category, String key) {
        String value = getStringValue(category, key);
        try {
            return new Integer(value);
        } catch (NumberFormatException e) {
            return null;
        }
    }

    public void load(InputStream in) throws IOException, ParseException {
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
                    propGroup = new HashMap<String, String>();
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
                        propGroup.put(key, value);
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
}
