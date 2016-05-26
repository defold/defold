package com.dynamo.cr.server.auth;

public class AuthenticationException extends RuntimeException {

    private static final long serialVersionUID = -7490514181359708448L;

    private String realm = null;

    public AuthenticationException(String message, String realm) {
        super(message);
        this.realm = realm;
    }

    public String getRealm() {
        return this.realm;
    }

}
