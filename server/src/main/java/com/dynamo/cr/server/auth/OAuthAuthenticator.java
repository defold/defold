package com.dynamo.cr.server.auth;

import com.dynamo.cr.server.util.LRUCache;
import com.google.api.client.http.GenericUrl;
import com.google.api.client.http.HttpRequest;
import com.google.api.client.http.HttpRequestFactory;
import com.google.api.client.http.HttpResponse;
import org.apache.commons.codec.binary.Hex;
import org.codehaus.jackson.map.DeserializationConfig.Feature;
import org.codehaus.jackson.map.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.ws.rs.core.UriBuilder;
import java.io.IOException;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;

/**
 * Authenticator. Associates login token with authentication requests, light-weight session management for the authentication process only.
 * This class simplifies the login token management and related authentication.
 * @author chmu
 *
 */
public class OAuthAuthenticator {

    protected static Logger logger = LoggerFactory.getLogger(OAuthAuthenticator.class);

    private SecureRandom secureRandom;
    // Map from loginToken -> Authentication
    private LRUCache<String, Authentication> authenticationsInProgress;

    public static class Authentication {
        Authentication(Identity identity, String accessToken) {
            this.identity = identity;
            this.accessToken = accessToken;
            this.expires = System.currentTimeMillis() + 1000 * 60 * 4;
        }
        public Identity identity;
        long expires;
        public String accessToken;
    }

    /**
     * Constructor
     * @param maxActiveLogins max active logins possible. If the number of active logins exceeds the maximum allowed
     * older entries are removed in least recently user order.
     */
    public OAuthAuthenticator(int maxActiveLogins) {
        authenticationsInProgress = new LRUCache<>(maxActiveLogins);
        try {
            secureRandom = SecureRandom.getInstance("SHA1PRNG");
        } catch (NoSuchAlgorithmException e1) {
            throw new RuntimeException(e1);
        }
    }

    /**
     * Create new login token
     * @return login login token
     */
    public synchronized String newLoginToken() {
        byte[] loginTokenBytes = new byte[16];
        secureRandom.nextBytes(loginTokenBytes);
        String loginToken = new String(Hex.encodeHex(loginTokenBytes));
        authenticationsInProgress.put(loginToken, null);
        return loginToken;
    }

    public synchronized void authenticate(HttpRequestFactory factory, String loginToken, String accessToken) throws IOException {
        ObjectMapper mapper = new ObjectMapper();
        mapper.configure(Feature.FAIL_ON_UNKNOWN_PROPERTIES, false);
        GenericUrl userInfoUrl = new GenericUrl(UriBuilder.fromUri("https://www.googleapis.com/oauth2/v1/userinfo").queryParam("access_token", accessToken).build());
        HttpRequest req = factory.buildGetRequest(userInfoUrl);
        HttpResponse userInfoResponse = req.execute();

        Identity identity = mapper.readValue(userInfoResponse.getContent(), Identity.class);

        // TODO: Do we have to validate jwt here?
        authenticationsInProgress.put(loginToken, new Authentication(identity, accessToken));
    }

    /**
     * Exchange login token for authentication cookie. The authentication cookie for login token
     * exchange is performed by on behalf of the client and after authentication is completed. The login token
     * is removed after this method is invoked.
     * @param loginToken login token to get authentication cookie for
     * @return {@link Authentication} if the login token is valid and authenticated. otherwise null
     */
    public synchronized Authentication exchange(String loginToken)  {
        Authentication authenticaton = authenticationsInProgress.get(loginToken);
        if (authenticaton == null) {
            logger.warn("Invalid login token {}", loginToken);
            return null;
        }
        long now = System.currentTimeMillis();
        if (now >= authenticaton.expires) {
            logger.warn("Login token {} expired", loginToken);
            return null;
        }

        authenticationsInProgress.remove(loginToken);
        return authenticaton;
    }
}
