package com.dynamo.cr.server.test;

public enum TestUser {
    JAMES("james.bond@mi9.com", "secret", "James", "Bond"),
    CARL("carl.content@gmail.com", "carl", "Carl", "Content"),
    JOE("joe.coder@gmail.com", "secret", "Joe", "Coder"),
    LISA("lisa.user@gmail.com", "secret", "Lisa", "User");

    public String email;
    public String password;
    public String firstName;
    public String lastName;

    TestUser(String email, String password, String firstName, String lastName) {
        this.email = email;
        this.password = password;
        this.firstName = firstName;
        this.lastName = lastName;
    }
}
