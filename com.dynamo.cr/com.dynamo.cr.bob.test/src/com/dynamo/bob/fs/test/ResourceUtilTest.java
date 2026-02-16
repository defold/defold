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

package com.dynamo.bob.fs.test;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import com.dynamo.bob.fs.ResourceUtil;

public class ResourceUtilTest {
    private static final String BUILD_DIR = "build/default";

    private Map<String, String> savedExtensionMapping;
    private Map<String, String> savedCachedPaths;
    private Set<String> savedMinifiedPathSet;
    private Set<String> savedMinifySupport;
    private boolean savedMinifyEnabled;
    private String savedBuildDirectory;

    private static class ResourceUtilAccessor extends ResourceUtil {
        static Map<String, String> snapshotExtensionMapping() {
            return new HashMap<>(extensionMapping);
        }

        static Map<String, String> snapshotCachedPaths() {
            return new HashMap<>(cachedPaths);
        }

        static Set<String> snapshotMinifiedPathSet() {
            return new HashSet<>(minifiedPaths);
        }

        static Set<String> snapshotMinifySupport() {
            return new HashSet<>(minifySupport);
        }

        static void restoreExtensionMapping(Map<String, String> snapshot) {
            extensionMapping.clear();
            extensionMapping.putAll(snapshot);
        }

        static void restoreCachedPaths(Map<String, String> snapshot) {
            cachedPaths.clear();
            cachedPaths.putAll(snapshot);
        }

        static void restoreMinifiedPaths(Set<String> snapshot) {
            minifiedPaths.clear();
            minifiedPaths.addAll(snapshot);
        }

        static void restoreMinifySupport(Set<String> snapshot) {
            minifySupport.clear();
            minifySupport.addAll(snapshot);
        }

        static void clearAll() {
            extensionMapping.clear();
            cachedPaths.clear();
            minifiedPaths.clear();
            minifySupport.clear();
        }

        static String getBuildDirectoryValue() {
            return buildDirectory;
        }

        static void setBuildDirectoryValue(String buildDir) {
            buildDirectory = buildDir;
        }
    }

    @Before
    public void setUp() {
        savedExtensionMapping = ResourceUtilAccessor.snapshotExtensionMapping();
        savedCachedPaths = ResourceUtilAccessor.snapshotCachedPaths();
        savedMinifiedPathSet = ResourceUtilAccessor.snapshotMinifiedPathSet();
        savedMinifySupport = ResourceUtilAccessor.snapshotMinifySupport();
        savedMinifyEnabled = ResourceUtil.isMinificationEnabled();
        savedBuildDirectory = ResourceUtilAccessor.getBuildDirectoryValue();

        ResourceUtilAccessor.clearAll();
        ResourceUtilAccessor.setBuildDirectoryValue(BUILD_DIR);
        ResourceUtil.enableMinification(false);
    }

    @After
    public void tearDown() {
        ResourceUtilAccessor.restoreExtensionMapping(savedExtensionMapping);
        ResourceUtilAccessor.restoreCachedPaths(savedCachedPaths);
        ResourceUtilAccessor.restoreMinifiedPaths(savedMinifiedPathSet);
        ResourceUtilAccessor.restoreMinifySupport(savedMinifySupport);
        ResourceUtil.enableMinification(savedMinifyEnabled);
        ResourceUtilAccessor.setBuildDirectoryValue(savedBuildDirectory);
    }

    @Test
    public void testMinifyDisabledAddsLeadingSlashForNonBuildPaths() {
        assertEquals("/foo.spc", ResourceUtil.minifyPath("foo.spc"));
        assertEquals("/foo.spc", ResourceUtil.minifyPath("/foo.spc"));
    }

    @Test
    public void testMinifyDisabledKeepsBuildPathsUnchanged() {
        String input = BUILD_DIR + "/foo.spc";
        assertEquals(input, ResourceUtil.minifyPath(input));
    }

    @Test
    public void testMinifyEnabledUnsupportedSuffixKeepsPathAndAddsSlashForNonBuild() {
        ResourceUtil.enableMinification(true);
        assertEquals("/foo.bar", ResourceUtil.minifyPath("foo.bar"));
        String buildInput = BUILD_DIR + "/foo.bar";
        assertEquals(buildInput, ResourceUtil.minifyPath(buildInput));
    }

    @Test
    public void testMinifyEnabledSupportedNonBuildPathIsMinifiedAndAbsolute() {
        ResourceUtil.enableMinification(true);
        ResourceUtil.registerMapping(".shbundle", ".spc");

        String result = ResourceUtil.minifyPath("foo.spc");
        assertTrue(result.startsWith("/"));
        assertFalse(result.startsWith("/" + BUILD_DIR + "/"));
        assertTrue(result.endsWith(".spc"));
        assertNotEquals("/foo.spc", result);
    }

    @Test
    public void testMinifyEnabledSupportedBuildPathIsMinifiedWithBuildPrefix() {
        ResourceUtil.enableMinification(true);
        ResourceUtil.registerMapping(".shbundle", ".spc");

        String input = BUILD_DIR + "/foo.spc";
        String result = ResourceUtil.minifyPath(input);
        assertTrue(result.startsWith(BUILD_DIR + "/"));
        assertFalse(result.startsWith("/" + BUILD_DIR + "/"));
        assertTrue(result.endsWith(".spc"));
        assertNotEquals(input, result);
    }
}
