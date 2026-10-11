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
import java.io.InputStream;
import java.net.InetSocketAddress;
import java.net.Proxy;
import java.net.ProxySelector;
import java.net.SocketAddress;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

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
import com.dynamo.bob.test.util.MockFileSystem;
import com.dynamo.bob.util.BobProjectProperties;

/**
 * Tests for HTML5Bundler.getUrlOrigin(), which decides if index.html should contain a
 * preconnect hint for the origin hosting the game archive (html5.archive_location_prefix).
 * A null result means "no hint", ie the archive is loaded from the same origin as index.html.
 */
public class HTML5BundlerTest {
    @Rule
    public TemporaryFolder temporaryFolder = new TemporaryFolder();

    private Project createHeapSizeProject() {
        MockFileSystem fileSystem = new MockFileSystem();
        fileSystem.addFile("/style.css", new byte[0]);
        Project project = new Project(fileSystem);
        project.getProjectProperties().putStringValue("html5", "cssfile", "/style.css");
        return project;
    }

    private void assertHeapSize(Project project, long expectedSize) throws IOException {
        Map<String, Map<String, Object>> propertiesMap = new HashMap<>();
        Map<String, Object> properties = new HashMap<>();
        new HTML5Bundler().updateManifestProperties(project, Platform.WasmWeb,
                project.getProjectProperties(), propertiesMap, properties);
        String heapSize = BundleHelper.formatResource(propertiesMap, properties,
                "{{DEFOLD_HEAP_SIZE}}".getBytes(StandardCharsets.UTF_8), "heap size");
        assertEquals(Long.toString(expectedSize), heapSize);
    }

    private void assertLoaderWebGPU(Platform platform, String architectures, String manifest, boolean expected) throws Exception {
        try (Project project = createHeapSizeProject()) {
            BobProjectProperties projectProperties = project.getProjectProperties();
            projectProperties.loadDefaultMetaFile();
            projectProperties.putStringValue("html5", "cssfile", "/style.css");
            project.setOption("platform", platform.getPair());
            project.setOption("architectures", architectures);
            if (manifest != null) {
                projectProperties.putStringValue("native_extension", "app_manifest", "graphics.appmanifest");
                project.getResource("graphics.appmanifest").setContent(manifest.getBytes(StandardCharsets.UTF_8));
            }
            project.configurePreBuildProjectOptions();

            Map<String, Map<String, Object>> propertiesMap = projectProperties.createTypedMap(
                    new BobProjectProperties.PropertyType[] {BobProjectProperties.PropertyType.BOOL});
            Map<String, Object> properties = new HashMap<>();
            properties.put("exe-name", "game");
            new HTML5Bundler().updateManifestProperties(project, platform, projectProperties, propertiesMap, properties);
            try (InputStream input = HTML5Bundler.class.getResourceAsStream("resources/web/dmloader.js")) {
                String loader = BundleHelper.formatResource(propertiesMap, properties, input.readAllBytes(), "dmloader.js");
                assertEquals(expected, loader.contains("navigator.gpu"));
                assertEquals(expected, loader.contains("probeWebGPUSupport"));
                assertEquals(expected, loader.contains("hasWebGPUSupport"));
            }
        }
    }

    // Default WebGL engines must omit all WebGPU probing, including pthread builds.
    @Test
    public void testWebGLLoaderOmitsWebGPUProbe() throws Exception {
        for (Platform platform : List.of(Platform.WasmWeb, Platform.WasmPthreadWeb)) {
            assertLoaderWebGPU(platform, platform.getPair(), null, false);
        }
    }

    // WebGPU symbols or libraries in generic and architecture-specific manifests must retain probing.
    @Test
    public void testWebGPUAppManifestKeepsProbe() throws Exception {
        for (Platform platform : List.of(Platform.WasmWeb, Platform.WasmPthreadWeb)) {
            for (String manifestPlatform : List.of("web", platform.getPair())) {
                for (String adapter : List.of("symbols: [GraphicsAdapterWebGPU]", "libs: [graphics_webgpu]")) {
                    assertLoaderWebGPU(platform, platform.getPair(),
                            "platforms:\n  " + manifestPlatform + ":\n    context:\n      " + adapter + "\n", true);
                }
            }
        }
    }

    // An explicitly excluded WebGPU adapter must not leave its probe in the generated loader.
    @Test
    public void testExcludedWebGPUOmitsProbe() throws Exception {
        assertLoaderWebGPU(Platform.WasmWeb, "wasm-web",
                "platforms:\n  web:\n    context:\n      symbols: [GraphicsAdapterWebGPU]\n" +
                "      excludeSymbols: [GraphicsAdapterWebGPU]\n", false);
    }

    // A shared loader must retain the probe when either bundled engine architecture includes WebGPU.
    @Test
    public void testWebGPUInPthreadArchitectureKeepsProbe() throws Exception {
        assertLoaderWebGPU(Platform.WasmWeb, "wasm-web,wasm_pthread-web",
                "platforms:\n  wasm_pthread-web:\n    context:\n      libs: [graphics_webgpu_wagyu]\n", true);
    }

    // An unset heap size must still render the default 256 MiB as a byte count.
    @Test
    public void testDefaultHeapSize() throws IOException {
        try (Project project = createHeapSizeProject()) {
            assertHeapSize(project, 268435456L);
        }
    }

    // Heap sizes at and above 2048 MiB must not overflow into negative or zero byte counts (#12753).
    @Test
    public void testHeapSizeDoesNotOverflow() throws IOException {
        try (Project project = createHeapSizeProject()) {
            int[] heapSizes = {32, 256, 2047, 2048, 4096};
            long[] expectedSizes = {33554432L, 268435456L, 2146435072L, 2147483648L, 4294967296L};
            for (int i = 0; i < heapSizes.length; ++i) {
                project.getProjectProperties().putIntValue("html5", "heap_size", heapSizes[i]);
                assertHeapSize(project, expectedSizes[i]);
            }
        }
    }

    // The deprecated byte count must override heap_size only when set_custom_heap_size is enabled.
    @Test
    public void testDeprecatedHeapSizeOverride() throws IOException {
        try (Project project = createHeapSizeProject()) {
            BobProjectProperties projectProperties = project.getProjectProperties();
            projectProperties.putIntValue("html5", "heap_size", 4096);
            projectProperties.putIntValue("html5", "custom_heap_size", 134217728);
            assertHeapSize(project, 4294967296L);
            projectProperties.putBooleanValue("html5", "set_custom_heap_size", true);
            assertHeapSize(project, 134217728L);
            projectProperties.putBooleanValue("html5", "set_custom_heap_size", false);
            assertHeapSize(project, 4294967296L);
        }
    }

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
