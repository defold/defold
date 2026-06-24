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

package com.dynamo.bob.util;

import org.junit.Test;

import java.net.URI;
import java.nio.charset.StandardCharsets;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

public class DependencyMetadataTest {

    @Test
    public void testAnonymizedUrlRemovesCredentialsAndQuery() throws Exception {
        assertEquals(
                "https://example.com:8443/path/to/lib.zip#fragment",
                DependencyMetadata.anonymizedUrl(new URI("https://user:secret@example.com:8443/path/to/lib.zip?token=abc#fragment")));
        assertEquals(
                "https://github.com/defold/extension-switch/archive/refs/tags/6.1.zip",
                DependencyMetadata.anonymizedUrl(new URI("https://__CITOKEN__@github.com/defold/extension-switch/archive/refs/tags/6.1.zip")));
    }

    @Test
    public void testReleaseVersion() {
        assertEquals("3.10.0", DependencyMetadata.releaseVersion(URI.create("https://github.com/defold/extension-spine/archive/refs/tags/3.10.0.zip")));
        assertEquals("v1.2.0", DependencyMetadata.releaseVersion(URI.create("https://github.com/defold/extension-simpledata/releases/download/v1.2.0/extension-simpledata.zip")));
        assertEquals("8.4.1", DependencyMetadata.releaseVersion(URI.create("https://github.com/company/extension-name/releases/8.4.1.zip")));
        assertEquals("Alpha_v2.2", DependencyMetadata.releaseVersion(URI.create("https://github.com/Jrayp/Moku/archive/Alpha_v2.2.zip")));
        assertNull(DependencyMetadata.releaseVersion(URI.create("https://github.com/defold/extension-spine/releases/latest/download/extension-spine.zip")));
        assertNull(DependencyMetadata.releaseVersion(URI.create("https://github.com/defold/template-empty/archive/master.zip")));
        assertNull(DependencyMetadata.releaseVersion(URI.create("https://github.com/defold/template-empty/archive/refs/heads/main.zip")));
    }

    @Test
    public void testFirstSha1() {
        assertEquals(
                "0123456789abcdef0123456789abcdef01234567",
                DependencyMetadata.firstSha1("commit 0123456789ABCDEF0123456789abcdef01234567 second fedcba9876543210fedcba9876543210fedcba98"));
        assertNull(DependencyMetadata.firstSha1("no sha1 here"));
        assertNull(DependencyMetadata.firstSha1("0123456789abcdef0123456789abcdef0123456789"));
    }

    @Test
    public void testMinifyJson() {
        byte[] minified = DependencyMetadata.minifyJson("[ {\n  \"url\" : \"https://example.com/library.zip\"\n} ]".getBytes(StandardCharsets.UTF_8));
        assertEquals("[{\"url\":\"https://example.com/library.zip\"}]", new String(minified, StandardCharsets.UTF_8));
    }
}
