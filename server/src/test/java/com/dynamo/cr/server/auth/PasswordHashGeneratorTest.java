package com.dynamo.cr.server.auth;

import org.junit.Test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

public class PasswordHashGeneratorTest {

    @Test
    public void passwordHashShouldBeValid() {
        String password = "super secret sauce!";
        String passwordHash = PasswordHashGenerator.generateHash(password);
        assertTrue(PasswordHashGenerator.validatePassword(password, passwordHash));
        assertFalse(PasswordHashGenerator.validatePassword("wrong password", passwordHash));
    }

}