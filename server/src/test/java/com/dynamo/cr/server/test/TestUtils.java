package com.dynamo.cr.server.test;

import com.dynamo.cr.server.model.User;

import javax.persistence.EntityManager;

public class TestUtils {

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

    public static User buildUser(TestUser testUser) {
        User user = new User();
        user.setEmail(testUser.email);
        user.setFirstName(testUser.firstName);
        user.setLastName(testUser.lastName);
        user.setPassword(testUser.password);
        return user;
    }
}
