package com.dynamo.cr.server.auth;

import org.codehaus.jackson.annotate.JsonProperty;

public class Identity {
    @JsonProperty("id")
    public String id;
    @JsonProperty("verified_email")
    public boolean verifiedEmail;
    @JsonProperty("email")
    public String email;
    @JsonProperty("given_name")
    public String firstName;
    @JsonProperty("family_name")
    public String lastName;
}
