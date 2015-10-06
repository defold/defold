package com.dynamo.crypt;

public class Crypt {

    private final static int NUM_ROUNDS = 32;

    private static int[] toIntArray(byte[] data, int n) {
        int[] result = new int[n >> 2];
        for (int i = 0; i < data.length; ++i) {
            result[i >>> 2] |= (0x000000ff & data[i]) << ((3 - (i & 3)) << 3);
        }
        return result;
    }

    private static byte[] toByteArray(int[] data) {
        int n = data.length << 2;
        byte[] result = new byte[n];

        for (int i = 0; i < n; ++i) {
            result[i] = (byte) (data[i >>> 2] >>> ((3 - (i & 3)) << 3));
        }
        return result;
    }

    private static int[] encrypt(int[] v, int[] key) {
        int sum = 0;
        int delta = 0x9e3779b9;
        int v0 = v[0];
        int v1 = v[1];
        for (int i = 0; i < NUM_ROUNDS; i++) {
            v0 += (((v1 << 4) ^ (v1 >>> 5)) + v1) ^ (sum + key[sum & 3]);
            sum += delta;
            v1 += (((v0 << 4) ^ (v0 >>> 5)) + v0) ^ (sum + key[(sum >>> 11) & 3]);
        }
        return new int[] { v0, v1 };
    }

    public static byte[] encryptCTR(byte[] data, byte[] key) {
        byte[] result = new byte[data.length];
        int[] counter = new int[2];
        byte[] enc_counter = new byte[8];
        int[] int_key = toIntArray(key, 16);

        for (int i = 0; i < data.length; i++) {
            if (i % 8 == 0) {
                enc_counter = toByteArray(encrypt(counter, int_key));
                // TODO: Include overflow for supporting more than 2^31 bytes of data.
                //       i.e. handling incrementing counter[0] as well.
                counter[1]++;
            }
            result[i] = (byte) ((data[i] ^ enc_counter[i % 8]) & 0xff);
        }
        return result;
    }

    public static byte[] decryptCTR(byte[] data, byte[] key) {
        return encryptCTR(data, key);
    }
}
