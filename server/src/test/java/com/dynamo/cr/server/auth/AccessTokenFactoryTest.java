package com.dynamo.cr.server.auth;

import org.junit.Test;

import static org.junit.Assert.assertTrue;

public class AccessTokenFactoryTest {

    @Test
    public void hashingShouldBeConsistent() {
        AccessTokenFactory accessTokenFactory = new AccessTokenFactory();
        String token = accessTokenFactory.create();
        String tokenHash1 = accessTokenFactory.generateTokenHash(token);
        String tokenHash2 = accessTokenFactory.generateTokenHash(token);
        assertTrue(accessTokenFactory.validateToken(token, tokenHash1));
        assertTrue(accessTokenFactory.validateToken(token, tokenHash2));
    }
}
