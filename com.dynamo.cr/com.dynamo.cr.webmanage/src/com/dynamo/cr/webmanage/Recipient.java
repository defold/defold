package com.dynamo.cr.webmanage;

import java.io.Serializable;

public class Recipient implements Serializable {

    private static final long serialVersionUID = 1L;
    private String email;
    private String key;

    public Recipient(String email, String key) {
        this.email = email;
        this.key = key;
    }

    public String getEmail() {
        return email;
    }

    public String getKey() {
        return key;
    }

}
