package com.dynamo.bob.test.util;

import static org.junit.Assert.assertEquals;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.math.BigInteger;

import org.junit.Test;

import com.dynamo.bob.util.MurmurHash;

public class MurmurHashTest {
    @Test
    public void testHash() throws Exception {
        InputStream input = getClass().getResourceAsStream("hashes.txt");
        BufferedReader reader = new BufferedReader(new InputStreamReader(input));
        String line = null;
        while ((line = reader.readLine()) != null) {
            String[] elems = line.split(" ");
            String s = elems[0];
            int hash32 = new BigInteger(elems[1], 16).intValue();
            long hash64 = new BigInteger(elems[2], 16).longValue();
            assertEquals(hash32, MurmurHash.hash32(s));
            assertEquals(hash64, MurmurHash.hash64(s));
        }
    }
}
