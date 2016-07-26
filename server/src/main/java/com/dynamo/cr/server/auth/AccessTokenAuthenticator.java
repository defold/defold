package com.dynamo.cr.server.auth;

import com.dynamo.cr.server.model.AccessToken;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.util.SystemTimeSource;
import com.dynamo.cr.server.util.TimeSource;

import javax.inject.Inject;
import java.time.Duration;
import java.time.temporal.ChronoUnit;
import java.util.Date;
import java.util.List;
import java.util.Optional;

public class AccessTokenAuthenticator {
    private static final Duration SESSION_PROLONG_DURATION = Duration.of(24, ChronoUnit.HOURS);
    private final AccessTokenFactory accessTokenFactory = new AccessTokenFactory();
    private final AccessTokenStore accessTokenStore;
    private TimeSource timeSource = new SystemTimeSource();

    @Inject
    public AccessTokenAuthenticator(AccessTokenStore accessTokenStore) {
        this.accessTokenStore = accessTokenStore;
    }

    /**
     * Creates a new session token that ,mnasdfwill expire if it is not used to authenticate within the
     * SESSION_PROLONG_DURATION.
     *
     * @param user User.
     * @param ip User's IP-address (if available).
     * @return New session token.
     */
    public String createSessionToken(User user, String ip) {
        return createToken(user, ip, generateSessionExpireDate());
    }

    /**
     * Creates a new lifetime access token (without any expire date). Any existing lifetime tokens are revoked, because
     * a user is only allowed to have at most one lifetime token at the time.
     *
     * @param user User.
     * @param ip User's IP-address (if available).
     * @return New lifetime token.
     */
    public String createLifetimeToken(User user, String ip) {
        revokeLifetimeTokens(user);
        return createToken(user, ip, null);
    }

    private String createToken(User user, String ip, Date expires) {
        String token = accessTokenFactory.create();
        String tokenHash = accessTokenFactory.generateTokenHash(token);
        AccessToken accessToken = new AccessToken(user, tokenHash, expires, timeSource.currentDate(), timeSource.currentDate(), ip);
        accessTokenStore.store(accessToken);
        return token;
    }

    private void revokeLifetimeTokens(User user) {
        List<AccessToken> accessTokens = accessTokenStore.find(user);

        accessTokens.stream()
                .filter(AccessToken::isLifetime)
                .forEach(accessTokenStore::delete);
    }

    /**
     * Authenticates an access token.
     *
     * @param user User.
     * @param token Access token.
     * @param ip User's IP-address (if available).
     * @return True if the the user's access token is valid, false otherwise.
     */
    public boolean authenticate(User user, String token, String ip) {
        List<AccessToken> tokens = accessTokenStore.find(user);

        deleteExpiredTokens(tokens);

        Optional<AccessToken> optional = tokens.stream()
                .filter(accessToken -> accessTokenFactory.validateToken(token, accessToken.getTokenHash()))
                .filter(accessToken -> !hasExpired(accessToken))
                .findAny();

        if (optional.isPresent()) {
            AccessToken accessToken = optional.get();
            if (!accessToken.isLifetime()) {
                accessToken.setExpires(generateSessionExpireDate());
            }
            accessToken.setLastUsed(timeSource.currentDate());
            accessToken.setIp(ip);
            accessTokenStore.store(accessToken);
            return true;
        }

        return false;
    }

    void setTimeSource(TimeSource timeSource) {
        this.timeSource = timeSource;
    }

    Duration getSessionProlongDuration() {
        return SESSION_PROLONG_DURATION;
    }

    private Date generateSessionExpireDate() {
        return Date.from(timeSource.currentInstant().plus(SESSION_PROLONG_DURATION));
    }

    private boolean hasExpired(AccessToken accessToken) {
        return accessToken.getExpires() != null && accessToken.getExpires().before(timeSource.currentDate());
    }

    private void deleteExpiredTokens(List<AccessToken> tokens) {
        tokens.stream().filter(this::hasExpired).forEach(accessTokenStore::delete);
    }
}
