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
    
    protected void setValue(String group, String key, String value) {
    	if (group != null && group.length() > 0) {
    		if (key != null && key.length() > 0) {
    			if (value != null && value.length() > 0) {
    				if (!this.properties.containsKey(group)) {
    					this.properties.put(group, new LinkedHashMap<String, String>());
    				}
    				
    				this.properties.get(group).put(key.toLowerCase(), value.trim());
    			} else {
    				if (this.properties.containsKey(group)) {
    					this.properties.get(group).remove(key);
    					if (this.properties.get(group).size() == 0) {
    						this.properties.remove(group);
    					}
    				}
    			}
    		}
    	}
    }

    protected String getValue(String group, String key) {
        if (validInput(group) && validInput(key)) {
            if (this.properties.containsKey(group)) {
                if (this.properties.get(group).containsKey(key)) {
                    return this.properties.get(group).get(key);
                }
            }
        }

        return null;
    }

    public void setManifestPublicKey(String value) {
        this.setValue("liveupdate", "publickey", value);
    }

    public String getManifestPublicKey() {
        String value = this.getValue("liveupdate", "publickey");
        return value != null ? value : "";
    }

    public void setManifestPrivateKey(String value) {
        this.setValue("liveupdate", "privatekey", value);
    }

    public String getManifestPrivateKey() {
        String value = this.getValue("liveupdate", "privatekey");
        return value != null ? value : "";
    }

    public void setSupportedVersions(String value) {
        this.setValue("liveupdate", "supported-versions", value);
    }

    public String getSupportedVersions() {
        String value = this.getValue("liveupdate", "supported-versions");
        return value != null ? value : "";
    }

    public void setMode(PublishMode value) {
    	this.setValue("liveupdate", "mode", value.toString());
    }
    
    public PublishMode getMode() {
        String value = this.getValue("liveupdate", "mode");
        return value != null ? PublishMode.valueOf(value) : null;
    }
    
    public void setAmazonCredentialProfile(String value) {
    	this.setValue("liveupdate", "amazon-credential-profile", value);
    }

    public String getAmazonCredentialProfile() {
    	return this.getValue("liveupdate", "amazon-credential-profile");
    }
    
    public void setAmazonBucket(String value) {
    	this.setValue("liveupdate", "amazon-bucket", value);
    }
    
    public String getAmazonBucket() {
        return this.getValue("liveupdate", "amazon-bucket");
    }

    public void setAmazonPrefix(String value) {
    	this.setValue("liveupdate", "amazon-prefix", value);
    }
    
    public String getAmazonPrefix() {
        return this.getValue("liveupdate", "amazon-prefix");
    }

    public void setZipFilepath(String value) {
    	this.setValue("liveupdate", "zip-filepath", value);
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
                } else if (parts.length == 1 && parts[0].length() > 0) {
                    settings.properties.get(currentGroup).put(parts[0].toLowerCase().trim(), "");
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
