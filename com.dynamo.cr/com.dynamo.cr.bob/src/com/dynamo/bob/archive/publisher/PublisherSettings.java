package com.dynamo.bob.archive.publisher;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.text.ParseException;
import java.util.LinkedHashMap;
import java.util.Map;

import org.apache.commons.io.IOUtils;

public class PublisherSettings {

    public enum PublishMode {
        Amazon, Defold, Zip
    };

    private Map<String, Map<String, String>> properties = new LinkedHashMap<String, Map<String, String>>();

    private boolean validInput(String val) {
        return val != null && val.length() > 0;
    }

    public void setValue(String group, String key, String value) {
        if (validInput(group) && validInput(key) && validInput(value)) {
            if (!this.properties.containsKey(group)) {
                this.properties.put(group, new LinkedHashMap<String, String>());
            }

            this.properties.get(group).put(key.toLowerCase().trim(), value.trim());
        }
    }

    public String getValue(String group, String key) {
        if (validInput(group) && validInput(key)) {
            if (this.properties.containsKey(group)) {
                if (this.properties.get(group).containsKey(key)) {
                    return this.properties.get(group).get(key);
                }
            }
        }

        return null;
    }

    public void removeValue(String group, String key) {
        if (validInput(group) && validInput(key)) {
            if (this.properties.containsKey(group)) {
                if (this.properties.get(group).containsKey(key)) {
                    this.properties.get(group).remove(key);
                }
            }
        }
    }

    public PublishMode getMode() {
        String value = this.getValue("liveupdate", "mode");
        if (value != null) {
            return PublishMode.valueOf(value);
        }

        return null;
    }

    public String getAwsAccessKey() {
        return this.getValue("liveupdate", "aws-access-key");
    }

    public String getAwsSecretKey() {
        return this.getValue("liveupdate", "aws-secret-key");
    }

    public String getAwsBucket() {
        return this.getValue("liveupdate", "aws-bucket");
    }

    public String getAwsPrefix() {
        return this.getValue("liveupdate", "aws-prefix");
    }

    public String getZipFilepath() {
        return this.getValue("liveupdate", "zip-filepath");
    }

    private static PublisherSettings doLoad(InputStream in) throws IOException, ParseException {
        PublisherSettings settings = new PublisherSettings();
        BufferedReader reader = new BufferedReader(new InputStreamReader(in));

        int cursor = 0;
        String currentGroup = null;
        String current = null;
        while ((current = reader.readLine()) != null) {
            current = current.trim();
            if (current.startsWith("[") && current.endsWith("]")) {
                currentGroup = current.substring(1, current.length() - 1).toLowerCase();
                if (!settings.properties.containsKey(current)) {
                    settings.properties.put(currentGroup, new LinkedHashMap<String, String>());
                }
            } else if (current.startsWith("[")) {
                throw new ParseException("invalid category: " + current, cursor);
            } else if (current.length() > 0 && currentGroup == null) {
                throw new ParseException("no initial category", 0);
            } else if (current.length() > 0) {
                String[] parts = current.split("=");
                if (parts.length == 2 && parts[0].length() > 0 && parts[1].length() > 0) {
                    settings.properties.get(currentGroup).put(parts[0].toLowerCase().trim(), parts[1].trim());
                } else {
                    throw new ParseException("invalid key/value-pair: " + current, cursor);
                }
            }

            cursor += current.length();
        }

        return settings;
    }

    public static PublisherSettings load(InputStream in) throws IOException, ParseException {
        try {
            return PublisherSettings.doLoad(in);
        } finally {
            IOUtils.closeQuietly(in);
        }
    }

    public static void save(PublisherSettings settings, File fhandle) {
        PrintWriter writer = null;
        try {
            writer = new PrintWriter(fhandle);
            for (String group : settings.properties.keySet()) {
                writer.println("[" + group + "]");
                for (String key : settings.properties.get(group).keySet()) {
                    String value = settings.properties.get(group).get(key);
                    writer.println(key + "=" + value);
                }
            }
            writer.close();
        } catch (IOException e) {
           // Nothing we can do here..
        } finally {
            IOUtils.closeQuietly(writer);
        }
    }

}
