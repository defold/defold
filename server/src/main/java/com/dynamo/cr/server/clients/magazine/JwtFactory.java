package com.dynamo.cr.server.clients.magazine;

import com.auth0.jwt.JWT;
import com.auth0.jwt.algorithms.Algorithm;

import java.security.interfaces.RSAPrivateKey;
import java.time.Instant;
import java.util.Date;

public class JwtFactory {
    private final RSAPrivateKey privateKey;

    public JwtFactory(RSAPrivateKey privateKey) {
        this.privateKey = privateKey;
    }

    public String create(String user, Instant expires, String contextPath, boolean writeAccess) {
        return JWT.create()
                .withSubject(user)
                .withExpiresAt(Date.from(expires))
                .withClaim("uri", contextPath)
                .withClaim("w", writeAccess)
                .sign(Algorithm.RSA256(privateKey));
    }
}
