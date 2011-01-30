package com.dynamo.cr.web;

import java.io.Serializable;

public class User implements Serializable {

    private static final long serialVersionUID = -6483583307028288009L;
    private String user;

    public User(String user) {
        this.user = user;
    }

    public String getUser() {
        return user;
    }

}
