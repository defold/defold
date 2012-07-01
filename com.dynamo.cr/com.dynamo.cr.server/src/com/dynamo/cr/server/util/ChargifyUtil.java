package com.dynamo.cr.server.util;

import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.List;
import java.util.Map;

import javax.ws.rs.core.MediaType;

import org.apache.commons.codec.binary.Hex;

import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.WebResource.Builder;
import com.sun.jersey.api.representation.Form;

public class ChargifyUtil {

    public static final String SIGNATURE_HEADER_NAME = "X-Chargify-Webhook-Signature";

    public static final String SIGNUP_SUCCESS_WH = "signup_success";
    public static final String SIGNUP_FAILURE_WH = "signup_failure";
    public static final String RENEWAL_FAILURE_WH = "renewal_failure";
    public static final String SUBSCRIPTION_STATE_CHANGE_WH = "subscription_state_change";

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
    public static String generateSignature(String key, String body) {
        if (digest == null) {
            try {
                digest = MessageDigest.getInstance("MD5");
            } catch (NoSuchAlgorithmException e) {
                throw new RuntimeException(e);
            }
        }
        digest.reset();
        digest.update(key.getBytes());
        digest.update(body.getBytes());
        return new String(Hex.encodeHex(digest.digest()));
    }

    private static String generateSignature(Form f, String sharedKey) throws UnsupportedEncodingException {
        // Format the form into the corresponding HTTP request body
        // Could not find a nice way to do this in jersey
        StringBuffer buffer = new StringBuffer();
        boolean empty = true;
        for (Map.Entry<String, List<String>> entry : f.entrySet()) {
            for (String value : entry.getValue()) {
                if (!empty) {
                    buffer.append("&");
                }
                String key = URLEncoder.encode(entry.getKey(), "UTF-8");
                value = URLEncoder.encode(value, "UTF-8");
                buffer.append(key).append("=").append(value);
                empty = false;
            }
        }
        String body = buffer.toString();
        return ChargifyUtil.generateSignature(sharedKey, body);
    }

    public static ClientResponse fakeWebhook(WebResource targetResource, String event, Form f, boolean sign,
            String sharedKey)
            throws UnsupportedEncodingException {
        f.add("event", event);

        Builder builder = targetResource.type(MediaType.APPLICATION_FORM_URLENCODED_TYPE)
                .accept(MediaType.APPLICATION_JSON_TYPE).entity(f);
        if (sign) {
            String signature = generateSignature(f, sharedKey);
            builder.header(ChargifyUtil.SIGNATURE_HEADER_NAME, signature);
        }
        return builder.post(ClientResponse.class);
    }
}
