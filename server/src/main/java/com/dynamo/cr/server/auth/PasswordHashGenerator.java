package com.dynamo.cr.server.auth;

import org.mindrot.jbcrypt.BCrypt;

public class PasswordHashGenerator {

    /**
     * Generates a hash for a clear text password using bcrypt.
     *
     * @param password Clear text password.
     * @return Hashed password.
     */
    public static String generateHash(String password) {
        return BCrypt.hashpw(password, BCrypt.gensalt());
    }

    /**
     * Validates a clear text password towards hashed password.
     *
     * @param password Clear text password.
     * @param passwordHash Password hash.
     * @return True if the password hash was generated from the same clear text password as provided, false otherwise.
     */
    public static boolean validatePassword(String password, String passwordHash) {
        return BCrypt.checkpw(password, passwordHash);
    }
}
