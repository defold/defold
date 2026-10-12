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

package com.dynamo.bob.bundle.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import java.io.File;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Proxy;
import java.net.ProxySelector;
import java.net.SocketAddress;
import java.net.URI;
import java.nio.file.Files;
import java.util.List;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import com.dynamo.bob.Bob;
import com.dynamo.bob.EngineArtifactsProvider;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.bundle.HTML5Bundler;
import com.dynamo.bob.fs.DefaultFileSystem;

/**
 * Tests for HTML5Bundler.getUrlOrigin(), which decides if index.html should contain a
 * preconnect hint for the origin hosting the game archive (html5.archive_location_prefix).
 * A null result means "no hint", ie the archive is loaded from the same origin as index.html.
 */
public class HTML5BundlerTest {
    @Rule
    public TemporaryFolder temporaryFolder = new TemporaryFolder();

    @Test
    public void testMissingPthreadEngineAbortsBundle() throws Exception {
        // Local Bob builds may still embed this engine. The download failure only
        // applies when it is absent, as in the default Bob distribution.
        assumeTrue(Bob.class.getResource("/libexec/wasm_pthread-web/dmengine_release.js") == null);
        Bob.init();
        assumeTrue(!new File(Bob.getRootFolder(), "wasm_pthread-web/dmengine_release.js").exists());

        File projectDir = temporaryFolder.newFolder("project");
        File buildDir = new File(projectDir, "build");
        assertTrue(buildDir.mkdirs());
        for (String name : BundleHelper.getArchiveFilenames(buildDir)) {
            Files.write(new File(buildDir, name).toPath(), new byte[] {1});
        }
        File bundleDir = temporaryFolder.newFolder("bundle");
        ProxySelector previousProxySelector = ProxySelector.getDefault();
        EngineArtifactsProvider.setCacheBase(temporaryFolder.newFolder("cache"));
        try (Project project = new Project(new DefaultFileSystem(), projectDir.getAbsolutePath(), "build")) {
            // Force a connection failure without contacting the engine archive.
            ProxySelector.setDefault(new ProxySelector() {
                @Override
                public List<Proxy> select(URI uri) {
                    return List.of(new Proxy(Proxy.Type.HTTP, new InetSocketAddress("127.0.0.1", 0)));
                }

                @Override
                public void connectFailed(URI uri, SocketAddress address, IOException error) {
                }
            });
            project.setOption("architectures", "wasm_pthread-web");
            project.setOption("variant", Bob.VARIANT_RELEASE);
            project.getProjectProperties().putStringValue("project", "title", "OfflineTest");
            try {
                new HTML5Bundler().bundleApplication(project, Platform.WasmPthreadWeb, bundleDir, () -> false);
                fail("Expected bundling to fail when the pthread engine cannot be downloaded");
            } catch (IOException e) {
                assertTrue(e.getMessage().contains("release engine for wasm_pthread-web (dmengine_release.js)"));
                assertTrue(e.getMessage().contains("Check your internet connection and try again."));
                assertFalse(new File(bundleDir, "OfflineTest/dmloader.js").exists());
            }
        } finally {
            ProxySelector.setDefault(previousProxySelector);
            EngineArtifactsProvider.setCacheBase(null);
        }
    }

    // The default value of html5.archive_location_prefix and other relative prefixes must not
    // produce a preconnect hint, since they are served from the same origin as index.html.
    @Test
    public void testRelativeArchiveLocationPrefixHasNoOrigin() {
        assertNull(HTML5Bundler.getUrlOrigin("archive"));
        assertNull(HTML5Bundler.getUrlOrigin("/archive"));
        assertNull(HTML5Bundler.getUrlOrigin("./archive"));
        assertNull(HTML5Bundler.getUrlOrigin("../foo/archive"));
        assertNull(HTML5Bundler.getUrlOrigin("some/nested/archive"));
        assertNull(HTML5Bundler.getUrlOrigin(""));
    }

    @Test
    public void testAbsoluteArchiveLocationPrefix() {
        assertEquals("https://cdn.example.com", HTML5Bundler.getUrlOrigin("https://cdn.example.com/games/mygame/archive"));
        assertEquals("http://cdn.example.com", HTML5Bundler.getUrlOrigin("http://cdn.example.com/archive"));
    }

    // A prefix without any path is a valid url and should still yield an origin
    @Test
    public void testAbsoluteArchiveLocationPrefixWithoutPath() {
        assertEquals("https://cdn.example.com", HTML5Bundler.getUrlOrigin("https://cdn.example.com"));
        assertEquals("https://cdn.example.com", HTML5Bundler.getUrlOrigin("https://cdn.example.com/"));
    }

    // A non default port is part of the origin and must be kept, or the browser would warm up
    // a connection to the wrong endpoint
    @Test
    public void testAbsoluteArchiveLocationPrefixWithPort() {
        assertEquals("https://cdn.example.com:8443", HTML5Bundler.getUrlOrigin("https://cdn.example.com:8443/archive"));
        assertEquals("http://localhost:8080", HTML5Bundler.getUrlOrigin("http://localhost:8080/archive"));
    }

    // A protocol relative prefix inherits the scheme of the page, and the hint should do the same
    @Test
    public void testProtocolRelativeArchiveLocationPrefix() {
        assertEquals("//cdn.example.com", HTML5Bundler.getUrlOrigin("//cdn.example.com/archive"));
        assertEquals("//cdn.example.com:8443", HTML5Bundler.getUrlOrigin("//cdn.example.com:8443/archive"));
    }

    // Everything the browser cannot preconnect to should be ignored rather than throw, so that a
    // surprising archive_location_prefix never breaks bundling
    @Test
    public void testUnsupportedSchemeHasNoOrigin() {
        assertNull(HTML5Bundler.getUrlOrigin("ftp://cdn.example.com/archive"));
        assertNull(HTML5Bundler.getUrlOrigin("file:///Users/me/archive"));
    }

    @Test
    public void testMalformedUrlHasNoOrigin() {
        assertNull(HTML5Bundler.getUrlOrigin("https://cdn.example.com/a b c"));
        assertNull(HTML5Bundler.getUrlOrigin("https://"));
        assertNull(HTML5Bundler.getUrlOrigin("::not a url::"));
    }

    @Test
    public void testNullUrlHasNoOrigin() {
        assertNull(HTML5Bundler.getUrlOrigin(null));
    }
}
