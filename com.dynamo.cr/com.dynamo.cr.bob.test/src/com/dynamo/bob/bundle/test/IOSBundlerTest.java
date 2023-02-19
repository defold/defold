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

import com.dynamo.bob.bundle.IOSBundler;

public class IOSBundlerTest {

    @Test
    public void testValidBundleIdentifier() {
        IOSBundler ib = new IOSBundler();

        // two or more segments
        assertTrue(ib.isValidBundleIdentifier("a.b"));
        assertTrue(ib.isValidBundleIdentifier("com.foo"));
        assertTrue(ib.isValidBundleIdentifier("com.foo.bar"));
        assertFalse(ib.isValidBundleIdentifier(""));
        assertFalse(ib.isValidBundleIdentifier("com"));
        assertFalse(ib.isValidBundleIdentifier("com."));
        assertFalse(ib.isValidBundleIdentifier("com.foo."));

        // numbers
        assertTrue(ib.isValidBundleIdentifier("com1.foo"));
        assertTrue(ib.isValidBundleIdentifier("com.foo1"));
        assertFalse(ib.isValidBundleIdentifier("1com.foo"));
        assertFalse(ib.isValidBundleIdentifier("com.1foo"));
        assertFalse(ib.isValidBundleIdentifier("123.456"));
        
        // underscore
        assertTrue(ib.isValidBundleIdentifier("com_.foo"));
        assertTrue(ib.isValidBundleIdentifier("com.foo_"));
        assertTrue(ib.isValidBundleIdentifier("c_m.f_o"));
        assertFalse(ib.isValidBundleIdentifier("_com.foo"));
        assertFalse(ib.isValidBundleIdentifier("com._foo"));

        // hypen
        assertTrue(ib.isValidBundleIdentifier("com-.foo"));
        assertTrue(ib.isValidBundleIdentifier("com.foo-"));
        assertTrue(ib.isValidBundleIdentifier("c-m.f-o"));
        assertFalse(ib.isValidBundleIdentifier("-com.foo"));
        assertFalse(ib.isValidBundleIdentifier("com.-foo"));

        // uppercase
        assertTrue(ib.isValidBundleIdentifier("A.B"));
        assertTrue(ib.isValidBundleIdentifier("CoM.fOo"));

        // only a-z, A-Z, 0-9, _
        assertFalse(ib.isValidBundleIdentifier("cöm.föö"));
    }
}
