// Copyright 2020-2023 The Defold Foundation
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

package com.dynamo.bob.bundle.test;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;

import org.junit.Test;

import com.dynamo.bob.bundle.AndroidBundler;

public class AndroidBundlerTest {

    @Test
    public void testValidPackageName() {
        AndroidBundler ab = new AndroidBundler();

        // two or more segments
        assertTrue(ab.isValidPackageName("a.b"));
        assertTrue(ab.isValidPackageName("com.foo"));
        assertTrue(ab.isValidPackageName("com.foo.bar"));
        assertFalse(ab.isValidPackageName(""));
        assertFalse(ab.isValidPackageName("com"));
        assertFalse(ab.isValidPackageName("com."));
        assertFalse(ab.isValidPackageName("com.foo."));

        // numbers
        assertTrue(ab.isValidPackageName("com1.foo"));
        assertTrue(ab.isValidPackageName("com.foo1"));
        assertFalse(ab.isValidPackageName("1com.foo"));
        assertFalse(ab.isValidPackageName("com.1foo"));
        assertFalse(ab.isValidPackageName("123.456"));
        
        // underscore
        assertTrue(ab.isValidPackageName("com_.foo"));
        assertTrue(ab.isValidPackageName("com.foo_"));
        assertTrue(ab.isValidPackageName("c_m.f_o"));
        assertFalse(ab.isValidPackageName("_com.foo"));
        assertFalse(ab.isValidPackageName("com._foo"));

        // uppercase
        assertTrue(ab.isValidPackageName("A.B"));
        assertTrue(ab.isValidPackageName("CoM.fOo"));

        // only a-z, A-Z, 0-9, _
        assertFalse(ab.isValidPackageName("cöm.föö"));
    }
}
