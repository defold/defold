package com.dynamo.cr.server.auth;

import java.security.Principal;

import com.dynamo.cr.server.model.User;

public class UserPrincipal implements Principal {

    private User user;

    public UserPrincipal(User user) {
        this.user = user;

    }

    @Override
    public String getName() {
        return user.getEmail();
    }

    public User getUser() {
        return user;
    }

}
