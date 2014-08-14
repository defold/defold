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

