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
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;

import org.apache.commons.io.FileUtils;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.ClassLoaderResourceScanner;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.util.FileUtil;
import com.dynamo.bob.bundle.BundleHelper.ResourceInfo;
import com.dynamo.bob.fs.ClassLoaderMountPoint;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ExtenderUtil;

public class BundleHelperTest {

    ClassLoaderMountPoint mp;

    @Before
    public void setUp() throws Exception {
        this.mp = new ClassLoaderMountPoint(null, "com/dynamo/bob/bundle/test/*", new ClassLoaderResourceScanner());
        this.mp.mount();
    }

    @After
    public void tearDown() throws Exception {
        this.mp.unmount();
    }

    // Thx, Java
    public static boolean compareStr(String str1, String str2) {
        return (str1 == null ? str2 == null : str1.equals(str2));
    }

    static protected boolean checkIssue(List<ResourceInfo> issues, String resource, int lineNumber, String severity, String message) {
        for ( ResourceInfo info : issues) {
            if (compareStr(info.resource, resource) && info.lineNumber == lineNumber && info.severity.equals(severity) && info.message.equals(message)) {
                return true;
            }
        }
        return false;
    }

    private static void printIssue(String resource, int lineNumber, String severity, String message) {
        System.out.printf("ISSUE: %s : %s : %s : %s\n", resource, lineNumber, severity, message);
    }

    static protected boolean expectIssueTrue(List<ResourceInfo> issues, String resource, int lineNumber, String severity, String message) {
        ResourceInfo info = null;
        if (resource == null) {
            System.out.println("Cannot test for a null resource");
            printIssue(resource, lineNumber, severity, message);
            return false;
        }
        for ( ResourceInfo i : issues) {
            if (compareStr(i.resource, resource)) {
                info = i;
                break;
            }
        }

        if (info == null) {
            System.out.println("No such resource found!");
            printIssue(resource, lineNumber, severity, message);
            return false;
        }

        boolean ret = info.lineNumber == lineNumber && info.severity.equals(severity) && info.message.equals(message);
        if (!ret) {
            System.out.println("expected");
            printIssue(resource, lineNumber, severity, message);
            System.out.println("but got");
            printIssue(info.resource, info.lineNumber, info.severity, info.message);
        }
        return ret;
    }

    static void debugIssues(List<ResourceInfo> issues) {
        for (ResourceInfo info : issues) {
            System.out.printf(String.format("ISSUE: %s : %d: %s - \"%s\"\n", info.resource, info.lineNumber, info.severity, info.message));
        }
    }

    @Test
    public void testProjectNameToBinaryName() {
        assertEquals("dmengine", BundleHelper.projectNameToBinaryName(""));
        assertEquals("dmengine", BundleHelper.projectNameToBinaryName(" "));
        assertEquals("dmengine", BundleHelper.projectNameToBinaryName("Лорем ипсум"));
        assertEquals("aao", BundleHelper.projectNameToBinaryName("åäö"));
        assertEquals("aao", BundleHelper.projectNameToBinaryName("âäó"));
        assertEquals("KyHoang", BundleHelper.projectNameToBinaryName("Kỳ Hoàng"));
        assertEquals("test_project", BundleHelper.projectNameToBinaryName("test_projectЛорем ипсум"));
        assertEquals("test_project", BundleHelper.projectNameToBinaryName("test_project"));
        assertEquals("testproject", BundleHelper.projectNameToBinaryName("test project"));
        assertEquals("testproject", BundleHelper.projectNameToBinaryName("test\nproject"));
    }

    @Test
    public void testErrorLog() throws IOException, CompileExceptionError {

        {
            IResource resource = this.mp.get("com/dynamo/bob/bundle/test/errorLogAndroid.txt");
            String log = new String(resource.getContent());
            List<ResourceInfo> issues = new ArrayList<ResourceInfo>();
            BundleHelper.parseLog("armv7-android", log, issues);

            assertTrue(expectIssueTrue(issues, "androidnative/src/main1.cpp", 37, "error", "In constructor '{anonymous}::AttachScope::AttachScope()':\n'Attach' was not declared in this scope\n     AttachScope() : m_Env(Attach())"));
            assertEquals(false, checkIssue(issues, null, 0, "warning", "[options] bootstrap class path not set in conjunction with -source 1.6"));
            assertTrue(expectIssueTrue(issues, "androidnative/src/main2.cpp", 17, "error", "'ubar' does not name a type\n ubar g_foo = 0;"));
            // Link error
            assertTrue(expectIssueTrue(issues, "androidnative/src/main3.cpp", 147, "error", "undefined reference to 'Foobar()'\ncollect2: error: ld returned 1 exit status"));
            // Main link error (missing extension symbol)
            assertTrue(expectIssueTrue(issues, "main4.cpp", 44, "error", "undefined reference to 'defos'\ncollect2: error: ld returned 1 exit status"));

            assertEquals(true, checkIssue(issues, null, 1, "error", "Uncaught translation error: java.lang.IllegalArgumentException: already added: Landroid/support/v4/app/ActionBarDrawerToggle;"));
            assertEquals(true, checkIssue(issues, null, 1, "error", "Uncaught translation error: java.lang.IllegalArgumentException: already added: Landroid/support/v4/app/ActionBarDrawerToggle$Delegate;"));

            // make sure it strips upload/packages from the path
            assertTrue(expectIssueTrue(issues, "ags_native/androidx-core-core-1.0.0/values/androidx-core-values.xml", 81, "error", "Attribute \"ttcIndex\" has already been defined\n"));
            assertTrue(expectIssueTrue(issues, "AndroidManifest.xml", 142, "error", "Error: No resource found that matches the given name (at 'value' with value '@integer/google_play_services_version')."));
        }
        {
            IResource resource = this.mp.get("com/dynamo/bob/bundle/test/errorLogWin32.txt");
            String log = new String(resource.getContent());
            List<ResourceInfo> issues = new ArrayList<ResourceInfo>();
            BundleHelper.parseLog("x86_64-win32", log, issues);

            assertTrue(expectIssueTrue(issues, "androidnative/src/main2.cpp", 17, "error", "missing type specifier - int assumed. Note: C++ does not support default-int"));
            assertTrue(expectIssueTrue(issues, "androidnative/src/main3.cpp", 17, "error", "syntax error: missing ';' before identifier 'g_foo'"));
            assertTrue(expectIssueTrue(issues, "main.cpp_0.o", 1, "error", "unresolved external symbol \"void __cdecl Foobar(void)\" (?Foobar@@YAXXZ) referenced in function \"enum dmExtension::Result __cdecl AppInitializeExtension(struct dmExtension::AppParams * __ptr64)\" (?AppInitializeExtension@@YA?AW4Result@dmExtension@@PEAUAppParams@2@@Z)"));

            assertTrue(expectIssueTrue(issues, "MathFuncsLib.lib", 1, "error", "MathFuncsLib.lib(MathFuncsLib.obj) : MSIL .netmodule or module compiled with /GL found; restarting link with /LTCG; add /LTCG to the link command line to improve linker performance"));
            assertEquals(true, checkIssue(issues, null, 1, "error", "fatal error C1900: Il mismatch between 'P1' version '20161212' and 'P2' version '20150812'"));
            assertEquals(true, checkIssue(issues, null, 1, "error", "LINK : fatal error LNK1257: code generation failed"));

            assertTrue(expectIssueTrue(issues, "king_device_id/src/kdid.cpp", 4, "fatal error", "Cannot open include file: 'unistd.h': No such file or directory"));

            // Clang errors
            assertTrue(expectIssueTrue(issues, "ProgramFilesx86/WindowsKits/8.1/Include/shared/ws2def.h", 905, "error", "pasting formed '\"Use \"\"ADDRINFOEXW\"', an invalid preprocessing token [-Winvalid-token-paste]\n"));
            // lld-link error
            assertTrue(expectIssueTrue(issues, "222613a7-2aea-4afd-8498-68f39f62468f-1.lib", 1, "error", "undefined symbol: _ERR_reason_error_string"));
            assertTrue(expectIssueTrue(issues, "222613a7-2aea-4afd-8498-68f39f62468f-2.lib", 1, "error", "undefined symbol: _ERR_clear_error"));
            assertTrue(expectIssueTrue(issues, "222613a7-2aea-4afd-8498-68f39f62468f-3.lib", 1, "error", "undefined symbol: _BIO_new_mem_buf"));
            assertEquals(true, checkIssue(issues, null, 1, "error", "could not open Crypt32.Lib.lib: No such file or directory"));
        }
        {
            IResource resource = this.mp.get("com/dynamo/bob/bundle/test/errorLogOSX.txt");
            String log = new String(resource.getContent());
            List<ResourceInfo> issues = new ArrayList<ResourceInfo>();
            BundleHelper.parseLog("x86_64-osx", log, issues);

            assertTrue(expectIssueTrue(issues, "androidnative/src/main1.cpp", 15, "error", "use of undeclared identifier 'Hello'\n    Hello();"));
            assertTrue(expectIssueTrue(issues, "androidnative/src/main2.cpp", 17, "error", "unknown type name 'ubar'\nubar g_foo = 0;"));
            assertEquals(true, checkIssue(issues, null, 1, "error", "Undefined symbols for architecture x86_64:\n  \"__Z6Foobarv\", referenced from:\n      __ZL22AppInitializeExtensionPN11dmExtension9AppParamsE in libb5622585-1c2d-4455-9d8d-c29f9404a475.a(main.cpp_0.o)"));
        }
        {
            IResource resource = this.mp.get("com/dynamo/bob/bundle/test/errorLogiOS.txt");
            String log = new String(resource.getContent());
            List<ResourceInfo> issues = new ArrayList<ResourceInfo>();
            BundleHelper.parseLog("arm64-ios", log, issues);

            assertTrue(expectIssueTrue(issues, "androidnative/src/main.cpp", 17, "error", "unknown type name 'ubar'\nubar g_foo = 0;"));
            assertEquals(true, checkIssue(issues, null, 1, "error", "Undefined symbols for architecture arm64:\n  \"__Z6Foobarv\", referenced from:\n      __ZL22AppInitializeExtensionPN11dmExtension9AppParamsE in lib44391c30-91a4-4faf-aef6-2dbc429af9ed.a(main.cpp_0.o)"));
            assertEquals(true, checkIssue(issues, null, 1, "error", "Invalid Defold SDK: 'b2ef3a19802728e76adf84d51d02e11d636791a3'"));
            assertEquals(true, checkIssue(issues, null, 1, "error", "Missing library 'engine'"));
        }

        {
            IResource resource = this.mp.get("com/dynamo/bob/bundle/test/errorLogHTML5.txt");
            String log = new String(resource.getContent());
            List<ResourceInfo> issues = new ArrayList<ResourceInfo>();
            BundleHelper.parseLog("js-web", log, issues);

            assertEquals(true, checkIssue(issues, "androidnative/src/main.cpp", 17, "error", "unknown type name 'ubar'\nubar g_foo = 0;"));
        }

        {
            IResource resource = this.mp.get("com/dynamo/bob/bundle/test/errorLogLinux.txt");
            String log = new String(resource.getContent());
            List<ResourceInfo> issues = new ArrayList<ResourceInfo>();
            BundleHelper.parseLog("x86_64-linux", log, issues);

            assertTrue(expectIssueTrue(issues, "androidnative/src/main1.cpp", 17, "error", "‘ubar’ does not name a type\n ubar g_foo = 0;"));
            assertTrue(expectIssueTrue(issues, "androidnative/src/main2.cpp", 166, "error", "undefined reference to `Foobar()'\ncollect2: error: ld returned 1 exit status"));
            assertEquals(true, checkIssue(issues, null, 1, "error", "cannot find -lsteam_api"));
        }
    }

    @Test
    public void testReadYaml() throws IOException {
        IResource resource = this.mp.get("com/dynamo/bob/bundle/test/test.yml");
        Map<String, Object> main = ExtenderUtil.readYaml(resource);
        Map<String, Object> platforms = (Map<String, Object>)main.getOrDefault("platforms", null);
        assertNotNull(platforms);
        Map<String, Object> platform = (Map<String, Object>)platforms.getOrDefault("android", null);
        assertNotNull(platform);
        Map<String, Object> bundle = (Map<String, Object>)platform.getOrDefault("bundle", null);
        assertNotNull(bundle);
        List<String> packages = (List<String>)bundle.getOrDefault("aaptExtraPackages", new ArrayList<String>());
        String[] expectedPackages = {"com.facebook"};
        assertEquals(Arrays.asList(expectedPackages), packages);
    }

    @Test
    public void testMergeManifestContext() throws IOException {
        IResource resourceA = this.mp.get("com/dynamo/bob/bundle/test/test.yml");
        Map<String, Object> manifestA = ExtenderUtil.readYaml(resourceA);
        IResource resourceB = this.mp.get("com/dynamo/bob/bundle/test/test2.yml");
        Map<String, Object> manifestB = ExtenderUtil.readYaml(resourceB);

        // Make them compatible (cannot merge strings)
        manifestA.remove("name");
        manifestB.remove("name");
        Map<String, Object> merged = ExtenderUtil.mergeManifestContext(manifestA, manifestB);
        assertNotNull(merged);

        Map<String, Object> platforms = (Map<String, Object>)merged.getOrDefault("platforms", null);
        assertNotNull(platforms);
        Map<String, Object> platform = (Map<String, Object>)platforms.getOrDefault("android", null);
        assertNotNull(platform);
        Map<String, Object> bundle = (Map<String, Object>)platform.getOrDefault("bundle", null);
        assertNotNull(bundle);

        List<String> packages = (List<String>)bundle.getOrDefault("aaptExtraPackages", new ArrayList<String>());
        String[] expectedPackages = {"com.facebook", "com.other.package"};
        assertEquals(Arrays.asList(expectedPackages), packages);
    }

    @Test
    public void testExcludeString() throws IOException {
        List<String> input = Arrays.asList(new String[]{"com.facebook", "com.other.package", "com.foobar.blah"});

        List<String> expressions = Arrays.asList(new String[]{"com.f(.*)"});
        List<String> result = BundleHelper.excludeItems(input, expressions);
        assertEquals(Arrays.asList(new String[]{"com.other.package"}), result);
    }

    @Test
    public void testValidAndroidPackageName() {
        // two or more segments
        assertTrue(BundleHelper.isValidAndroidPackageName("a.b"));
        assertTrue(BundleHelper.isValidAndroidPackageName("com.foo"));
        assertTrue(BundleHelper.isValidAndroidPackageName("com.foo.bar"));
        assertFalse(BundleHelper.isValidAndroidPackageName(""));
        assertFalse(BundleHelper.isValidAndroidPackageName("com"));
        assertFalse(BundleHelper.isValidAndroidPackageName("com."));
        assertFalse(BundleHelper.isValidAndroidPackageName("com.foo."));

        // numbers
        assertTrue(BundleHelper.isValidAndroidPackageName("com1.foo"));
        assertTrue(BundleHelper.isValidAndroidPackageName("com.foo1"));
        assertFalse(BundleHelper.isValidAndroidPackageName("1com.foo"));
        assertFalse(BundleHelper.isValidAndroidPackageName("com.1foo"));
        assertFalse(BundleHelper.isValidAndroidPackageName("123.456"));
        
        // underscore
        assertTrue(BundleHelper.isValidAndroidPackageName("com_.foo"));
        assertTrue(BundleHelper.isValidAndroidPackageName("com.foo_"));
        assertTrue(BundleHelper.isValidAndroidPackageName("c_m.f_o"));
        assertFalse(BundleHelper.isValidAndroidPackageName("_com.foo"));
        assertFalse(BundleHelper.isValidAndroidPackageName("com._foo"));

        // uppercase
        assertTrue(BundleHelper.isValidAndroidPackageName("A.B"));
        assertTrue(BundleHelper.isValidAndroidPackageName("CoM.fOo"));

        // only a-z, A-Z, 0-9, _
        assertFalse(BundleHelper.isValidAndroidPackageName("cöm.föö"));
    }

    @Test
    public void testValidBundleIdentifier() {
        // two or more segments
        assertTrue(BundleHelper.isValidAppleBundleIdentifier("a.b"));
        assertTrue(BundleHelper.isValidAppleBundleIdentifier("com.foo"));
        assertTrue(BundleHelper.isValidAppleBundleIdentifier("com.foo.bar"));
        assertFalse(BundleHelper.isValidAppleBundleIdentifier(""));
        assertFalse(BundleHelper.isValidAppleBundleIdentifier("com"));
        assertFalse(BundleHelper.isValidAppleBundleIdentifier("com."));
        assertFalse(BundleHelper.isValidAppleBundleIdentifier("com.foo."));

        // numbers
        assertTrue(BundleHelper.isValidAppleBundleIdentifier("com1.foo"));
        assertTrue(BundleHelper.isValidAppleBundleIdentifier("com.foo1"));
        assertFalse(BundleHelper.isValidAppleBundleIdentifier("1com.foo"));
        assertFalse(BundleHelper.isValidAppleBundleIdentifier("com.1foo"));
        assertFalse(BundleHelper.isValidAppleBundleIdentifier("123.456"));
        
        // underscore
        assertTrue(BundleHelper.isValidAppleBundleIdentifier("com_.foo"));
        assertTrue(BundleHelper.isValidAppleBundleIdentifier("com.foo_"));
        assertTrue(BundleHelper.isValidAppleBundleIdentifier("c_m.f_o"));
        assertFalse(BundleHelper.isValidAppleBundleIdentifier("_com.foo"));
        assertFalse(BundleHelper.isValidAppleBundleIdentifier("com._foo"));

        // hypen
        assertTrue(BundleHelper.isValidAppleBundleIdentifier("com-.foo"));
        assertTrue(BundleHelper.isValidAppleBundleIdentifier("com.foo-"));
        assertTrue(BundleHelper.isValidAppleBundleIdentifier("c-m.f-o"));
        assertFalse(BundleHelper.isValidAppleBundleIdentifier("-com.foo"));
        assertFalse(BundleHelper.isValidAppleBundleIdentifier("com.-foo"));

        // uppercase
        assertTrue(BundleHelper.isValidAppleBundleIdentifier("A.B"));
        assertTrue(BundleHelper.isValidAppleBundleIdentifier("CoM.fOo"));

        // only a-z, A-Z, 0-9, _
        assertFalse(BundleHelper.isValidAppleBundleIdentifier("cöm.föö"));
    }

    private void createMockFile(File dir, String name) throws IOException {
        File file = new File(dir, name);
        FileUtil.deleteOnExit(file);
        FileUtils.copyInputStreamToFile(new ByteArrayInputStream("dummy".getBytes()), file);
    }

    @Test
    public void testCollectSharedLibrariesLinux() throws IOException {
        File tempDir = Files.createTempDirectory("test_linux_shared_collect").toFile();
        try {
            createMockFile(tempDir, "libfoo.so");
            createMockFile(tempDir, "libbar.so");
            createMockFile(tempDir, "notlib.txt");
            createMockFile(tempDir, "libtest.a");

            Set<String> result = BundleHelper.collectSharedLibraries(Platform.X86_64Linux, tempDir)
                    .map(path -> path.getFileName().toString())
                    .collect(Collectors.toSet());

            assertEquals(2, result.size());
            assertTrue(result.contains("libfoo.so"));
            assertTrue(result.contains("libbar.so"));
            assertFalse(result.contains("notlib.txt"));
            assertFalse(result.contains("libtest.a"));
        } finally {
            FileUtils.deleteDirectory(tempDir);
        }
    }

    @Test
    public void testCollectSharedLibrariesWindows() throws IOException {
        File tempDir = Files.createTempDirectory("test_windows_shared_collect").toFile();
        try {
            createMockFile(tempDir, "foo.dll");
            createMockFile(tempDir, "bar.dll");
            createMockFile(tempDir, "libfoo.dll");
            createMockFile(tempDir, "test.exe");

            Set<String> result = BundleHelper.collectSharedLibraries(Platform.X86_64Win32, tempDir)
                    .map(path -> path.getFileName().toString())
                    .collect(Collectors.toSet());

            assertEquals(3, result.size());
            assertTrue(result.contains("foo.dll"));
            assertTrue(result.contains("bar.dll"));
            assertTrue(result.contains("libfoo.dll"));
            assertFalse(result.contains("test.exe"));
        } finally {
            FileUtils.deleteDirectory(tempDir);
        }
    }

    @Test
    public void testCollectSharedLibrariesMacOS() throws IOException {
        File tempDir = Files.createTempDirectory("test_mac_shared_collect").toFile();
        try {
            createMockFile(tempDir, "libfoo.dylib");
            createMockFile(tempDir, "libbar.dylib");
            createMockFile(tempDir, "foo.dylib"); // not match
            createMockFile(tempDir, "libtest.a");

            Set<String> result = BundleHelper.collectSharedLibraries(Platform.X86_64MacOS, tempDir)
                    .map(path -> path.getFileName().toString())
                    .collect(Collectors.toSet());

            assertEquals(2, result.size());
            assertTrue(result.contains("libfoo.dylib"));
            assertTrue(result.contains("libbar.dylib"));
            assertFalse(result.contains("foo.dylib"));
            assertFalse(result.contains("libtest.a"));
        } finally {
            FileUtils.deleteDirectory(tempDir);
        }
    }

    @Test
    public void testCollectSharedLibrariesEmptyDir() throws IOException {
        File emptyDir = Files.createTempDirectory("test_collection").toFile();
        try {
            Set<String> result = BundleHelper.collectSharedLibraries(Platform.X86_64Linux, emptyDir)
                    .map(path -> path.getFileName().toString())
                    .collect(Collectors.toSet());
            assertEquals(0, result.size());
        } finally {
            FileUtils.deleteDirectory(emptyDir);
        }

        File nonExistent = new File("/non_existent_dir" + System.currentTimeMillis());
        Set<String> result = BundleHelper.collectSharedLibraries(Platform.X86_64Linux, nonExistent)
                .map(path -> path.getFileName().toString())
                .collect(Collectors.toSet());
        assertEquals(0, result.size());
    }

    @Test
    public void testCopySharedLibraries() throws IOException {
        File sourceDir = Files.createTempDirectory("test_shared_copy_src").toFile();
        File targetDir = Files.createTempDirectory("test_shared_copy_dst").toFile();
        try {
            createMockFile(sourceDir, "libfoo.so");
            createMockFile(sourceDir, "libbar.so");

            BundleHelper.copySharedLibraries(Platform.X86_64Linux, sourceDir, targetDir);

            File copiedFoo = new File(targetDir, "libfoo.so");
            File copiedBar = new File(targetDir, "libbar.so");
            assertTrue(copiedFoo.exists());
            assertTrue(copiedBar.exists());
            assertEquals("dummy", FileUtils.readFileToString(copiedFoo, "UTF-8"));
            assertEquals("dummy", FileUtils.readFileToString(copiedBar, "UTF-8"));
        } finally {
            FileUtils.deleteDirectory(sourceDir);
            FileUtils.deleteDirectory(targetDir);
        }
    }
}
