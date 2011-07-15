package com.dynamo.cr.server.openid;

import java.util.EnumMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.apache.commons.codec.binary.Base64;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * OpenID association representation. Represents the association between relying party and OpenID provider.
 * The association holds the shared secret and association handle among others.
 * @author chmu
 *
 */
public class Association {

    /**
     * Supported association keys
     */
    public enum Key {
        ns,
        session_type,
        assoc_type,
        assoc_handle,
        expires_in,
        mac_key
    }

    private static Logger logger = LoggerFactory.getLogger(Association.class);

    private EnumMap<Association.Key, String> values;
    private long expires;
    private byte[] key;
    private static Key[] requiredKeys = new Key[] {Association.Key.assoc_handle, Association.Key.expires_in, Association.Key.mac_key};

    private Association(EnumMap<Association.Key, String> values) {
        this.values = values;
        this.expires = System.currentTimeMillis() + Integer.parseInt(this.values.get(Key.expires_in)) * 1000;
        this.key = Base64.decodeBase64(values.get(Key.mac_key));
    }

    /**
     * Get the Key#mac_key base64 decoded
     * @return decoded key
     */
    public byte[] getKey() {
        return key;
    }

    /**
     * Check if the association has expired
     * @return true if expired
     */
    public boolean isExpired() {
        return expires <= System.currentTimeMillis();
    }

    /**
     * Get association value given a key
     * @param key key to get association value for
     * @return value or null if the mapping doesn't exists
     */
    public String get(Association.Key key) {
        return this.values.get(key);
    }

    @Override
    public String toString() {
        return String.format("Association(accoc_handle=%s, expires=%d)", get(Key.assoc_handle).toString(), expires);
    }

    /**
     * Create association from provider response. It is verifid that assoc_handle expires_in and mac_key are present. If
     * not and {@link OpenIDException} will be thrown.
     * @param response provider response
     * @return association
     * @throws OpenIDException
     */
    public static Association create(String response) throws OpenIDException {
        Pattern keyValue = Pattern.compile("(.*?):(.*?)");
        String[] lines = response.replace("\r", "").split("\n");
        EnumMap<Association.Key, String> associationValues = new EnumMap<Association.Key, String>(Association.Key.class);
        for (String line : lines) {
            Matcher matcher = keyValue.matcher(line);
            if (matcher.matches()) {

                String key = matcher.group(1);
                String value = matcher.group(2);
                try {
                    Association.Key enumKey = Association.Key.valueOf(key);
                    associationValues.put(enumKey, value);
                } catch (IllegalArgumentException e) {
                    logger.warn("WARNING: Unsupported key: {}", key);
                }
            }
        }

        // Ensure that we have all keys we are interested in.
        for (Association.Key key : requiredKeys) {
            if (!associationValues.containsKey(key)) {
                throw new OpenIDException(String.format("Association response is missing required key '%s'", key.toString()));
            }
        }

        return new Association(associationValues);
    }
}