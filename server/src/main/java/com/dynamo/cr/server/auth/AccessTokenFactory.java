package com.dynamo.cr.server.auth;

import org.mindrot.jbcrypt.BCrypt;

import java.util.UUID;

public class AccessTokenFactory {

    public String create() {
        return UUID.randomUUID().toString();
    }

    public String generateTokenHash(String token) {
        return BCrypt.hashpw(token, BCrypt.gensalt());
    }

    public boolean validateToken(String token, String tokenHash) {
        return BCrypt.checkpw(token, tokenHash);
    }
}
