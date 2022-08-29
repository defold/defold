// Copyright 2020-2022 The Defold Foundation
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

package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.net.URISyntaxException;
import java.util.Arrays;
import java.util.Collection;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.zip.ZipFile;
import java.util.zip.ZipOutputStream;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import java.security.CodeSource;
import com.defold.extender.client.ExtenderResource;
import com.dynamo.bob.ClassLoaderScanner;
import com.dynamo.bob.ClassLoaderResourceScanner;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.ZipMountPoint;
import com.dynamo.bob.fs.FileSystemWalker;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.test.util.MockFileSystem;


public class BundleResourcesTest {

    private MockFileSystem fileSystem;
    private Project project;
    private ZipMountPoint mp;

    @Rule
    public TemporaryFolder tmpFolder = new TemporaryFolder();

    @After
    public void tearDown() {
        this.project.dispose();
    }

    public BundleResourcesTest() throws IOException, URISyntaxException {
        this.fileSystem = new MockFileSystem();
        this.fileSystem.setBuildDirectory("");
        this.project = new Project(this.fileSystem);
        this.project.clearProjectProperties();

        ClassLoaderScanner scanner = new ClassLoaderScanner();
        project.scan(scanner, "com.dynamo.bob");
        project.scan(scanner, "com.dynamo.bob.pipeline");

        CodeSource src = getClass().getProtectionDomain().getCodeSource();
        String jarPath = new File(src.getLocation().toURI()).getAbsolutePath();
        this.mp = new ZipMountPoint(null, jarPath, false);
        this.mp.mount();

        addResourceDirectory("/testextension");
        addResourceDirectory("/testappmanifest");
    }

    private void addFile(String file, String source) {
        addFile(file, source.getBytes());
    }

    private void addFile(String file, byte[] content) {
        this.fileSystem.addFile(file, content);
    }

    private void addDirectory(String path) {
        this.fileSystem.addDirectory(path);
    }


    class Walker extends FileSystemWalker {

        private String basePath;
        private ZipMountPoint mp;

        public Walker(ZipMountPoint mp, String basePath) {
            this.mp = mp;
            this.basePath = basePath;
            if (basePath.startsWith("/")) {
                this.basePath = basePath.substring(1, basePath.length());
            }
        }

        @Override
        public void handleFile(String path, Collection<String> results) {
            String first = path.substring(0, path.indexOf("/"));
            if (first.equals(basePath)) {
                try {
                    IResource resource = this.mp.get(path);
                    path = path.substring(basePath.length()+1);
                    addFile("/" + path, resource.getContent());
                } catch (IOException e) {

                }
            }
        }

        @Override
        public boolean handleDirectory(String path, Collection<String> results) {
            path = FilenameUtils.normalize(path, true);
            if (path.endsWith("/")) {
                path = path.substring(0, path.length()-1);
            }
            String first = path.contains("/") ? path.substring(0, path.indexOf("/")) : path;
            if (!first.equals(basePath)) {
                return false;
            }
            path = path.substring(basePath.length(), path.length());
            addDirectory(path);
            return true;
        }
    }

    private void addResourceDirectory(String dir) {
        List<String> results = new ArrayList<String>(1024);
        this.mp.walk(dir, new Walker(this.mp, dir), results);
    }

    @Test
    public void testFindExtensionFolders() throws Exception {

        List<String> folders = ExtenderUtil.getExtensionFolders(project);
        assertEquals(1, folders.size());
        assertEquals("extension1", folders.get(0));

        // Add one more extension folder at root
        addFile("extension2/ext.manifest", "name: \"extension2\"");
        folders = ExtenderUtil.getExtensionFolders(project);
        assertEquals(2, folders.size());
        assertEquals("extension1", folders.get(0));
        assertEquals("extension2", folders.get(1));

        // Add one more extension folder in a nested subfolder
        addFile("subfolder/extension3/ext.manifest", "name: \"extension3\"");
        folders = ExtenderUtil.getExtensionFolders(project);
        assertEquals(3, folders.size());
        assertEquals("extension1", folders.get(0));
        assertEquals("extension2", folders.get(1));
        assertEquals("subfolder/extension3", folders.get(2));

    }

    @Test
    public void testcollectBundleResources() throws Exception {
        // Make sure we exclude stuff from extension1
        project.getProjectProperties().putStringValue("project", "bundle_exclude_resources", "/extension1/res/common/collision.txt,/extension1/res/common/subdir/subdirtest.txt");

        project.getProjectProperties().putStringValue("project", "bundle_resources", "/restest1");
        Map<String, IResource> resourceMap = ExtenderUtil.collectBundleResources(project, Arrays.asList(Platform.getHostPlatform()));
        assertEquals(2, resourceMap.size());
        assertTrue(resourceMap.containsKey("collision.txt"));
        assertTrue(resourceMap.containsKey("test.txt"));

        project.getProjectProperties().putStringValue("project", "bundle_resources", "/restest2");
        resourceMap = ExtenderUtil.collectBundleResources(project, Arrays.asList(Platform.getHostPlatform()));
        assertEquals(3, resourceMap.size());
    }

    @Test
    public void testExtensionResources() throws Exception {

        // Should find bundle resources inside the extension1 folder
        Map<String, IResource> resourceMap = ExtenderUtil.collectBundleResources(project, Arrays.asList(Platform.getHostPlatform()));
        assertEquals(2, resourceMap.size());
        assertTrue(resourceMap.containsKey("collision.txt"));
    }

    @Test
    public void testInvalidPath() throws Exception {

        // Add project property for bundle resources
        project.getProjectProperties().putStringValue("project", "bundle_resources", "/does_not_exist/");
        Map<String, IResource> resourceMap = ExtenderUtil.collectBundleResources(project, Arrays.asList(Platform.getHostPlatform()));

        // Will only contain collision.txt and subdirtest.txt from the extension directory.
        assertEquals(2, resourceMap.size());
        assertTrue(resourceMap.containsKey("collision.txt"));
        assertTrue(resourceMap.containsKey("subdir/subdirtest.txt"));
    }

    @Test
    public void testCollisionExcludes() throws Exception {

        // Add project property for bundle resources
        project.getProjectProperties().putStringValue("project", "bundle_resources", "/restest1/");

        // Exclude the conflicting file from bundle_resources
        project.getProjectProperties().putStringValue("project", "bundle_exclude_resources", "/restest1/common/collision.txt");
        Map<String, IResource> resourceMap = ExtenderUtil.collectBundleResources(project, Arrays.asList(Platform.getHostPlatform()));
        assertEquals(3, resourceMap.size());
        assertTrue(resourceMap.containsKey("collision.txt"));

        // Exclude the conflicting file from extension
        project.getProjectProperties().putStringValue("project", "bundle_exclude_resources", "/extension1/res/common/collision.txt");
        resourceMap = ExtenderUtil.collectBundleResources(project, Arrays.asList(Platform.getHostPlatform()));
        assertEquals(3, resourceMap.size());
        assertTrue(resourceMap.containsKey("collision.txt"));
    }

    @Test
    public void testNonSlashProjectSetting() throws Exception {

        // Add project property for bundle resources
        project.getProjectProperties().putStringValue("project", "bundle_exclude_resources", "/extension1/res/common/collision.txt,/extension1/res/common/subdir/subdirtest.txt");

        // Test "old" way of specifying custom resources without leading slash (ie non absolute)
        project.getProjectProperties().putStringValue("project", "bundle_resources", "restest1/");
        Map<String, IResource> resourceMap = ExtenderUtil.collectBundleResources(project, Arrays.asList(Platform.getHostPlatform()));
        assertEquals(2, resourceMap.size());
    }

    @Test
    public void testInvalidProjectSetting() throws Exception {

        // Add project property for bundle resources
        project.getProjectProperties().putStringValue("project", "bundle_exclude_resources", "/extension1/res/common/collision.txt,/extension1/res/common/subdir/subdirtest.txt");

        // Test non existing project path
        project.getProjectProperties().putStringValue("project", "bundle_resources", "/not_valid/");
        Map<String, IResource> resourceMap = ExtenderUtil.collectBundleResources(project, Arrays.asList(Platform.getHostPlatform()));
        assertEquals(0, resourceMap.size());
    }

    @Test
    public void testWriteToDisk() throws Exception {

        Map<String, IResource> resourceMap = ExtenderUtil.collectBundleResources(project, Arrays.asList(Platform.getHostPlatform()));
        File folder = tmpFolder.newFolder();

        ExtenderUtil.writeResourcesToDirectory(resourceMap, folder);

        // Output folder should contain: collision.txt and subdir/subdirtest.txt
        Collection<File> outputFiles = FileUtils.listFiles(folder, null, true);
        assertEquals(2, outputFiles.size());
        assertTrue(outputFiles.contains(new File(FilenameUtils.concat(folder.getAbsolutePath(), "collision.txt"))));
        assertTrue(outputFiles.contains(new File(FilenameUtils.concat(folder.getAbsolutePath(), "subdir/subdirtest.txt"))));
    }

    @Test
    public void testWriteToZip() throws Exception {

        Map<String, IResource> resourceMap = ExtenderUtil.collectBundleResources(project, Arrays.asList(Platform.getHostPlatform()));

        // Write entries to temp zip file
        File tmpZipFile = tmpFolder.newFile();
        ZipOutputStream zipOut = null;
        try {
            zipOut = new ZipOutputStream(new FileOutputStream(tmpZipFile));
            ExtenderUtil.writeResourcesToZip(resourceMap, zipOut);
        } finally {
            IOUtils.closeQuietly(zipOut);
        }

        // Read temp zip file and assert
        ZipFile zipFile = new ZipFile(tmpZipFile);
        assertEquals(2, zipFile.size());
        assertTrue(zipFile.getEntry("collision.txt") != null);
        assertTrue(zipFile.getEntry("subdir/subdirtest.txt") != null);
        zipFile.close();
    }

    // Platform specific tests
    @Test
    public void testPlatform() throws Exception {

        // Test data
        Map<Platform, String[]> expected = new HashMap<Platform, String[]>();
        expected.put(Platform.X86_64MacOS, new String[] { "osx.txt", "x86_64-osx.txt" });
        expected.put(Platform.X86_64Linux, new String[] { "linux.txt", "x86_64-linux.txt" });
        expected.put(Platform.X86Win32, new String[] { "win32.txt", "x86-win32.txt" });
        expected.put(Platform.X86_64Win32, new String[] { "win32.txt", "x86_64-win32.txt" });
        expected.put(Platform.Armv7Android, new String[] { "android.txt" });
        expected.put(Platform.Arm64Ios, new String[] { "ios.txt", "arm64-ios.txt" });
        expected.put(Platform.JsWeb, new String[] { "web.txt" });

        // Should find bundle resources inside the extension1 folder
        project.getProjectProperties().putStringValue("project", "bundle_resources", "/restest2/");

        Iterator<Map.Entry<Platform, String[]>> it = expected.entrySet().iterator();
        while (it.hasNext()) {
            Map.Entry<Platform, String[]> entry = (Map.Entry<Platform, String[]>)it.next();
            Platform expectedPlatform = entry.getKey();
            String[] expectedFiles = entry.getValue();

            Map<String, IResource> resourceMap = ExtenderUtil.collectBundleResources(project, Arrays.asList(expectedPlatform));

            // +3 size since collision.txt, common.txt subdir/subdirtest.txt always included.
            assertEquals(expectedFiles.length + 3, resourceMap.size());
            assertTrue(resourceMap.containsKey("collision.txt"));
            assertTrue(resourceMap.containsKey("common.txt"));
            assertTrue(resourceMap.containsKey("subdir/subdirtest.txt"));

            for (int i = 0; i < expectedFiles.length; ++i) {
                String expectedFile = expectedFiles[i];
                assertTrue(resourceMap.containsKey(expectedFile));
            }
        }

    }

    private ExtenderResource findInResourceList(List<ExtenderResource> list, String path) {

        for (ExtenderResource resource : list) {
            if (resource.getPath().equals(path)) {
                return resource;
            }
        }

        return null;
    }

    // Extension source collecting
    @Test
    public void testExtensionSources() throws Exception {
        List<ExtenderResource> resources = ExtenderUtil.getExtensionSources(project, Platform.X86_64MacOS, null);
        assertEquals(7, resources.size());

        assertTrue(findInResourceList(resources, "_app/app.manifest") != null);
        assertTrue(findInResourceList(resources, "extension1/ext.manifest") != null);
        assertTrue(findInResourceList(resources, "extension1/src/extension1.cpp") != null);
        assertTrue(findInResourceList(resources, "extension1/lib/common/common.a") != null);
        assertTrue(findInResourceList(resources, "extension1/lib/x86_64-osx/x86_64-osx.a") != null);
        assertTrue(findInResourceList(resources, "extension1/res/common/collision.txt") != null);
        assertTrue(findInResourceList(resources, "extension1/res/common/subdir/subdirtest.txt") != null);

        Map<String, String> appmanifestOptions = new HashMap<String,String>();
        appmanifestOptions.put("baseVariant", "release");
        resources = ExtenderUtil.getExtensionSources(project, Platform.Arm64Ios, appmanifestOptions);
        assertEquals(6, resources.size());

        assertTrue(findInResourceList(resources, "extension1/ext.manifest") != null);
        assertTrue(findInResourceList(resources, "extension1/src/extension1.cpp") != null);
        assertTrue(findInResourceList(resources, "extension1/lib/common/common.a") != null);
        assertTrue(findInResourceList(resources, "extension1/res/common/collision.txt") != null);
        assertTrue(findInResourceList(resources, "extension1/res/common/subdir/subdirtest.txt") != null);
        ExtenderResource appManifest = findInResourceList(resources, ExtenderUtil.appManifestPath);
        String synthesizedManifest = new String(appManifest.getContent());
        String expectedManifest = "";
        expectedManifest += "context:" + System.getProperty("line.separator");
        expectedManifest += "    baseVariant: release" + System.getProperty("line.separator");
        assertEquals(synthesizedManifest, expectedManifest);
    }

    @Test
    public void testAppManifestVariant() throws Exception {
        Map<String, String> appmanifestOptions = new HashMap<String,String>();
        appmanifestOptions.put("baseVariant", "debug");

        project.getProjectProperties().putStringValue("native_extension", "app_manifest", "myapp.appmanifest");
        List<ExtenderResource> resources = ExtenderUtil.getExtensionSources(project, Platform.X86_64MacOS, appmanifestOptions);
        assertEquals(7, resources.size());
        ExtenderResource appManifest = findInResourceList(resources, ExtenderUtil.appManifestPath);
        String patchedManifest = new String(appManifest.getContent());
        String expectedManifest = "";
        expectedManifest += "context:" + System.getProperty("line.separator");
        expectedManifest += "    baseVariant: debug" + System.getProperty("line.separator");
        assertTrue(patchedManifest.length() > expectedManifest.length());
        assertEquals(patchedManifest.substring(0, expectedManifest.length()), expectedManifest);
    }

}
