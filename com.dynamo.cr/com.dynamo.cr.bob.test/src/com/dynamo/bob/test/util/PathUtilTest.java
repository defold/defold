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

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import com.dynamo.bob.util.PathUtil;

public class PathUtilTest {

    @Before
    public void setUp() throws Exception {
    }

    @After
    public void tearDown() throws Exception {
    }

    private boolean match(String path, String pattern) {
        return PathUtil.wildcardMatch(path, pattern);
    }

    @Test
    public void testMatch() throws Exception {
        String pattern = "**"; // anything
        assertTrue(match("a.txt", pattern));
        assertTrue(match("a/a.txt", pattern));
        assertTrue(match("a/a/a.txt", pattern));

        pattern = "**/*"; // anything
        assertTrue(match("a.txt", pattern));
        assertTrue(match("a/a.txt", pattern));
        assertTrue(match("a/a/a.txt", pattern));

        pattern = "*/**"; // anything under a directory
        assertFalse(match("a.txt", pattern));
        assertTrue(match("a/a.txt", pattern));
        assertTrue(match("a/a/a.txt", pattern));

        pattern = "a.txt"; // exact
        assertTrue(match("a.txt", pattern));
        assertFalse(match("a/a.txt", pattern));
        assertFalse(match("a/a/a.txt", pattern));

        pattern = "*/a.txt"; // exact under one directory
        assertFalse(match("a.txt", pattern));
        assertTrue(match("a/a.txt", pattern));
        assertFalse(match("a/a/a.txt", pattern));

        pattern = "**/a.txt"; // exact
        assertTrue(match("a.txt", pattern));
        assertTrue(match("a/a.txt", pattern));
        assertTrue(match("a/a/a.txt", pattern));
        pattern = "*/*/a.txt"; // exact under two directories
        assertFalse(match("a.txt", pattern));
        assertFalse(match("a/a.txt", pattern));
        assertTrue(match("a/a/a.txt", pattern));
        pattern = "*/**/a.txt"; // under at least one directory
        assertFalse(match("a.txt", pattern));
        assertTrue(match("a/a.txt", pattern));
        assertTrue(match("a/a/a.txt", pattern));
        pattern = "**/a/**"; // "a" directory must be present
        assertFalse(match("a.txt", pattern));
        assertTrue(match("a/a.txt", pattern));
        assertTrue(match("a/a/a.txt", pattern));
        pattern = "*/a/**"; // "a" directory under one directory
        assertFalse(match("a.txt", pattern));
        assertFalse(match("a/a.txt", pattern));
        assertTrue(match("a/a/a.txt", pattern));
    }
}

