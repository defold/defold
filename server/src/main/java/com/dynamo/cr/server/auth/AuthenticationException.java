package com.dynamo.cr.server.auth;

public class AuthenticationException extends RuntimeException {

    /**
     *
     */
    private static final long serialVersionUID = -7490514181359708448L;

    public AuthenticationException(String message, String realm) {
        super(message);
        this.realm = realm;
    }

    private String realm = null;

    public String getRealm() {
        return this.realm;
    }

}
