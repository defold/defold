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

import java.util.Arrays;
import java.util.Collections;
import java.util.function.Predicate;

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

    private static Predicate<String> projPathPredicate(String... patterns) {
        return PathUtil.makeProjPathPredicate(Arrays.asList(patterns));
    }

    @Test
    public void testProjPathPredicateEmpty() {
        Predicate<String> pred = PathUtil.makeProjPathPredicate(Collections.emptyList());
        assertFalse(pred.test("/a"));
        assertFalse(pred.test("/a/b.txt"));
    }

    @Test
    public void testProjPathPredicateLiteral() {
        Predicate<String> pred = projPathPredicate("/a", "/b.c");
        assertTrue(pred.test("/a"));
        assertTrue(pred.test("/a/b"));
        assertTrue(pred.test("/a/b/c.txt"));
        assertFalse(pred.test("/ab"));
        assertFalse(pred.test("/aba"));
        assertFalse(pred.test("/ab/a"));
        assertTrue(pred.test("/b.c"));
        assertTrue(pred.test("/b.c/d"));
        assertFalse(pred.test("/bXc"));
    }

    @Test
    public void testProjPathPredicateWildcards() {
        Predicate<String> pred = projPathPredicate("/levels/*/tiled");
        assertTrue(pred.test("/levels/1/tiled"));
        assertTrue(pred.test("/levels/1/tiled/a.txt"));
        assertFalse(pred.test("/levels"));
        assertFalse(pred.test("/levels/1"));
        assertFalse(pred.test("/levels/tiled"));
        assertFalse(pred.test("/levelsX/1/tiled"));
        assertFalse(pred.test("/levels/1/tiledX"));
        assertFalse(pred.test("/levels/1/other.txt"));

        pred = projPathPredicate("/levels/*");
        assertTrue(pred.test("/levels/a.txt"));
        assertTrue(pred.test("/levels/a/b.txt"));
        assertFalse(pred.test("/levels"));

        pred = projPathPredicate("/a?c");
        assertTrue(pred.test("/abc"));
        assertFalse(pred.test("/ac"));
        assertFalse(pred.test("/a/c"));

        pred = projPathPredicate("/levels/**");
        assertTrue(pred.test("/levels"));
        assertTrue(pred.test("/levels/a/b/c.txt"));
        assertFalse(pred.test("/levelsX"));

        pred = projPathPredicate("/**/tiled");
        assertTrue(pred.test("/tiled"));
        assertTrue(pred.test("/a/tiled"));
        assertTrue(pred.test("/a/b/tiled/c.txt"));
        assertFalse(pred.test("/a/b/untiled"));

        pred = projPathPredicate("/a/**/b");
        assertTrue(pred.test("/a/b"));
        assertTrue(pred.test("/a/x/y/b"));
        assertFalse(pred.test("/a/xb"));

        pred = projPathPredicate("/a**b");
        assertTrue(pred.test("/ab"));
        assertTrue(pred.test("/a/x/b"));
        assertFalse(pred.test("/a/x/c"));
    }

    @Test
    public void testProjPathPredicateRegexControlCharacters() {
        Predicate<String> pred = projPathPredicate("/a.b/*", "/dir(1)/*.png", "/[x]/*", "/a+b/*", "/c$/*", "/d\\e/*", "/^f/*");
        assertTrue(pred.test("/a.b/k.txt"));
        assertFalse(pred.test("/aXb/k.txt"));
        assertTrue(pred.test("/dir(1)/a.png"));
        assertFalse(pred.test("/dir1/a.png"));
        assertFalse(pred.test("/dir(1)/aXpng"));
        assertTrue(pred.test("/[x]/k.txt"));
        assertFalse(pred.test("/x/k.txt"));
        assertTrue(pred.test("/a+b/k.txt"));
        assertFalse(pred.test("/aab/k.txt"));
        assertTrue(pred.test("/c$/k.txt"));
        assertFalse(pred.test("/c/k.txt"));
        assertTrue(pred.test("/d\\e/k.txt"));
        assertFalse(pred.test("/de/k.txt"));
        assertTrue(pred.test("/^f/k.txt"));
        assertFalse(pred.test("/f/k.txt"));
    }

    @Test
    public void testProjPathPredicateIsCaseSensitive() {
        Predicate<String> pred = projPathPredicate("/Levels", "/Assets/*.png", "/**/Generated");
        assertTrue(pred.test("/Levels"));
        assertTrue(pred.test("/Levels/a.txt"));
        assertFalse(pred.test("/levels"));
        assertFalse(pred.test("/levels/a.txt"));
        assertFalse(pred.test("/LEVELS"));
        assertTrue(pred.test("/Assets/a.png"));
        assertFalse(pred.test("/assets/a.png"));
        assertFalse(pred.test("/Assets/a.PNG"));
        assertTrue(pred.test("/a/Generated/b.txt"));
        assertFalse(pred.test("/a/generated/b.txt"));
    }
}
