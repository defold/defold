package com.dynamo.cr.web;

import org.apache.wicket.Request;
import org.apache.wicket.authentication.AuthenticatedWebSession;
import org.apache.wicket.authorization.strategies.role.Roles;

public class Session extends AuthenticatedWebSession {

    private static final long serialVersionUID = -3573895895174366629L;
    private User user = null;

    public Session(Request request) {
        super(request);
    }

    @Override
    public boolean authenticate(String username, String password) {
        user = new User(username);
        //username.equals("wicket") && password.equals("wicket");
        return true;
    }

    @Override
    public Roles getRoles() {
        if (isSignedIn()) {
            return new Roles(Roles.USER);
        }
        return null;
    }

    public User getUser() {
        return user;
    }

}
