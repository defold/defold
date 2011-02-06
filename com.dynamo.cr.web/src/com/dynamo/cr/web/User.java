package com.dynamo.cr.web;

import java.io.Serializable;
import java.net.URI;

public class User implements Serializable {

    private static final long serialVersionUID = -6483583307028288009L;
    private long userId;
    private String user;
    private String password;
    private URI resourceUri;

    public User(long userId, String user, String password, URI resourceUri) {
        this.userId = userId;
        this.user = user;
        this.password = password;
        this.resourceUri = resourceUri;
    }

    public long getUserId() {
        return userId;
    }

    public String getUser() {
        return user;
    }

    public String getPassword() {
        return password;
    }

    public URI getResourceUri() {
        return resourceUri;
    }

}
