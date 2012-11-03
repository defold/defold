package com.dynamo.cr.server.auth;

import java.net.URI;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;

import org.apache.commons.codec.binary.Hex;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.common.cache.LRUCache;
import com.dynamo.cr.server.openid.OpenID;
import com.dynamo.cr.server.openid.OpenIDException;
import com.dynamo.cr.server.openid.OpenIDIdentity;

/**
 * OpenID authenticator. Associates login token with authentication requests, light-weight session management for the authentication process only.
 * This class simplifies the login token management and related authentication.
 * @author chmu
 *
 */
public class OpenIDAuthenticator {

    protected static Logger logger = LoggerFactory.getLogger(OpenIDAuthenticator.class);

    private SecureRandom secureRandom;
    private LRUCache<String, Authenticator> authenticationsInProgress;

    public static class Authenticator {
        private String authCookie;
        private OpenIDIdentity identity;

        public Authenticator(String authCookie, OpenIDIdentity identity) {
            this.authCookie = authCookie;
            this.identity = identity;
        }

        public String getAuthCookie() {
            return authCookie;
        }

        public OpenIDIdentity getIdentity() {
            return identity;
        }
    }

    /**
     * Constructor
     * @param maxActiveLogins max active logins possible. If the number of active logins exceeds the maximum allowed
     * older entries are removed in least recently user order.
     */
    public OpenIDAuthenticator(int maxActiveLogins) {
        authenticationsInProgress = new LRUCache<String, Authenticator>(maxActiveLogins);
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

    /**
     * Authenticate OpenID response and associate login token with authentication cookie. Similar to {@link OpenID#verify(URI)} but also
     * associates the authentication cookie to the login token for later retrieval with the {@link OpenIDAuthenticator#exchange(String)}
     * @param uri URI response from OpenID provider
     * @param loginToken login token associated with authentication
     * @return
     * @throws OpenIDException if authentication fails
     */
    public synchronized OpenIDIdentity authenticate(OpenID openID, URI uri, String loginToken) throws OpenIDException {
        if (!authenticationsInProgress.containsKey(loginToken)) {
            logger.warn("Invalid login token {}", loginToken);
            throw new OpenIDException("Invalid login token");
        }

        OpenIDIdentity identity = openID.verify(uri);
        String authentication = AuthCookie.login(identity.getEmail());
        authenticationsInProgress.put(loginToken, new Authenticator(authentication, identity));
        return identity;
    }

    /**
     * Exchange login token for authentication cookie. The authentication cookie for login token
     * exchange is performed by on behalf of the client and after authentication is completed. The login token
     * is removed after this method is invoked.
     * @param loginToken login token to get authentication cookie for
     * @return {@link Authenticator} if the login token is valid and authenticated
     * @throws OpenIDException if login token is invalid
     */
    public synchronized Authenticator exchange(String loginToken) throws OpenIDException {
        Authenticator authenticator = authenticationsInProgress.get(loginToken);
        if (authenticator == null) {
            logger.warn("Invalid login token {}", loginToken);
            throw new OpenIDException("Invalid login token");
        }
        authenticationsInProgress.remove(loginToken);
        return authenticator;
    }

}
