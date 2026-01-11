// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

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
