package com.dynamo.cr.server.auth;

import com.google.common.io.BaseEncoding;
import com.google.common.net.UrlEscapers;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;

/**
 * Authenticator that implements the Discourse SSO authentication algorithm described at
 * https://meta.discourse.org/t/official-single-sign-on-for-discourse/13045
 */
public class DiscourseSSOAuthenticator {
    private final String key;

    /**
     * Constructor.
     *
     * @param key Secret key used to verify requests and sign responses. Note, this key needs to be the same as the SSO
     *            secret configured in the corresponding Discourse instance.
     */
    public DiscourseSSOAuthenticator(String key) {
        this.key = key;
    }

    /**
     * Verifies a signed request.
     *
     * @param data Request data.
     * @param signature Request signature.
     * @return True if the signature is valid for the data provided, false otherwise.
     */
    public boolean verifySignedRequest(String data, String signature) {
        return signature.equals(generateSignature(data));
    }

    /**
     * Generates a signature for the data provided.
     *
     * @param data Data to sign.
     * @return Signature of the data provided in base 16.
     */
    public String generateSignature(String data) {
        try {
            Mac mac = Mac.getInstance("HmacSHA256");
            SecretKeySpec key = new SecretKeySpec(this.key.getBytes(), "HmacSHA256");
            mac.init(key);
            byte[] bytes = mac.doFinal(data.getBytes());
            return BaseEncoding.base16().encode(bytes).toLowerCase();
        } catch (NoSuchAlgorithmException | InvalidKeyException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Creates a response to SSO authentication request. First, a nonce is extracted from the request. After that the
     * response payload is created and base 64 encoded.
     *
     * @param requestData Request data.
     * @param name User's name.
     * @param username User's username.
     * @param email User's email.
     * @param externalId User's identifier in authenticating system (Defold).
     * @param requireActivation True if Discourse need to verify email, true if authenticated system takes care of that.
     * @return Response payload to send to Discourse.
     */
    public String createResponsePayload(String requestData, String name, String username, String email, String externalId, boolean requireActivation) {

        String nonce = extractNonce(requestData);

        String payload = String.format(
                "nonce=%s&name=%s&username=%s&email=%s&external_id=%s&require_activation=%s",
                UrlEscapers.urlFormParameterEscaper().escape(nonce),
                UrlEscapers.urlFormParameterEscaper().escape(name),
                UrlEscapers.urlFormParameterEscaper().escape(username),
                UrlEscapers.urlFormParameterEscaper().escape(email),
                UrlEscapers.urlFormParameterEscaper().escape(externalId),
                String.valueOf(requireActivation)
        );

        return new String(Base64.encode(payload.getBytes()));
    }

    /**
     * Extracts nonce from request payload. Request payload is promised to be base 64 encoded and have the form
     * nonce=[nonce data]
     *
     * @param payload Request payload.
     * @return Nonce.
     */
    private String extractNonce(String payload) {
        return Base64.base64Decode(payload).substring(6);
    }
}
