package com.dynamo.cr.server.test;

import com.dynamo.cr.server.model.User;

import javax.persistence.EntityManager;

public class TestUtils {

    public enum TestUser {
        JAMES("james.bond@mi9.com", "secret", "James", "Bond");

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

    public static void createTestUsers(EntityManager entityManager) {
        for (TestUser testUser : TestUser.values()) {
            User user = new User();
            user.setEmail(testUser.email);
            user.setPassword(testUser.password);
            user.setFirstName(testUser.firstName);
            user.setLastName(testUser.lastName);
            entityManager.persist(user);
        }
    }
}
