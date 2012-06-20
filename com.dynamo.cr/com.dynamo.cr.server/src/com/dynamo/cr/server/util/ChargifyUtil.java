package com.dynamo.cr.server.util;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

public class ChargifyUtil {

    public static final String SIGNATURE_HEADER_NAME = "X-Chargify-Webhook-Signature";

    private static MessageDigest digest = null;

    /**
     * Generates a signature as: md5(key + body)
     *
     * @param key
     *            Shared key
     * @param body
     *            Request body
     * @return signature for authentication
     */
    public static byte[] generateSignature(byte[] key, byte[] body) {
        if (digest == null) {
            try {
                digest = MessageDigest.getInstance("MD5");
            } catch (NoSuchAlgorithmException e) {
                throw new RuntimeException(e);
            }
        }
        digest.reset();
        digest.update(key);
        digest.update(body);
        return digest.digest();
    }
}
