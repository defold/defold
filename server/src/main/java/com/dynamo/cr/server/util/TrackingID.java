package com.dynamo.cr.server.util;

import java.nio.ByteBuffer;

import javax.crypto.Cipher;
import javax.crypto.SecretKey;
import javax.crypto.spec.SecretKeySpec;
import javax.xml.bind.DatatypeConverter;

public class TrackingID {
    private static final String BLOWFISH = "Blowfish";
    private static final String BLOWFISH_CHIPHER = "Blowfish/ECB/NoPadding";
    private static byte[] KEY = DatatypeConverter.parseHexBinary("d6ba554f307779f3");
    private static int PREFIX = 0x00def01d;

    private static byte[] encrypt(byte[] plainText) {
        try {
            SecretKey secretKey = new SecretKeySpec(KEY, BLOWFISH);
            Cipher cipher = Cipher.getInstance(BLOWFISH_CHIPHER);
            cipher.init(Cipher.ENCRYPT_MODE, secretKey);
            return cipher.doFinal(plainText);
        } catch (Exception e) {
            throw new RuntimeException("Unable to encrypt", e);
        }
    }

    public static String trackingID(int projectId) {
        ByteBuffer plain = ByteBuffer.allocate(8);
        plain.putInt(PREFIX);
        plain.putInt(projectId);
        byte[] encrypted = encrypt(plain.array());
        return DatatypeConverter.printHexBinary(encrypted);
    }

}
