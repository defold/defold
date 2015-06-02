package com.dynamo.cr.server.auth;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

public class AuthToken {

    private static final String secret = "^!pe45Q9bj$wTb4lkbzSCM3jY5O9BDIxmDW";

    public static String login(String email) {
        MessageDigest sha1;
        try {
            sha1 = MessageDigest.getInstance("SHA1");
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }

        String auth = String.format("%s%s", secret, email);
        sha1.update(auth.getBytes());
        byte[] digest = sha1.digest();
        StringBuilder sb = new StringBuilder();
        for (byte b : digest) {
            sb.append(Integer.toHexString(0xff & b));
        }

        return sb.toString();
    }

    public static boolean auth(String email, String cookie) {
        return login(email).equals(cookie);
    }

}
