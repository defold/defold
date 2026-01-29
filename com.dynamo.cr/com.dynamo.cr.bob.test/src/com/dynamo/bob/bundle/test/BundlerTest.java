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

import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.file.FileSystem;
import java.nio.file.FileSystems;
import java.nio.file.Files;
import java.nio.file.Path;

import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.HashMap;
import java.util.List;
import java.util.Set;
import java.util.zip.ZipFile;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;


import javax.imageio.ImageIO;

import org.apache.commons.configuration2.ex.ConfigurationException;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.filefilter.DirectoryFileFilter;
import org.apache.commons.io.filefilter.RegexFileFilter;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;

import com.dynamo.bob.ClassLoaderScanner;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.MultipleCompileException;
import com.dynamo.bob.NullProgress;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.util.FileUtil;
import com.dynamo.bob.archive.ArchiveBuilder;
import com.dynamo.bob.archive.ManifestBuilder;
import com.dynamo.bob.archive.publisher.NullPublisher;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.liveupdate.proto.Manifest.HashAlgorithm;

@RunWith(Parameterized.class)
public class BundlerTest {

    private String contentRoot;
    private String outputDir;
    private String contentRootUnused;
    private Platform platform;

    private final String ANDROID_MANIFEST = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        + "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" package=\"com.example\" android:versionCode=\"1\">"
        + "  <application android:label=\"Minimal Android Application\">"
        + "    <activity android:name=\".MainActivity\" android:label=\"Hello World\">"
        + "      <intent-filter>"
        + "        <action android:name=\"android.intent.action.MAIN\" />"
        + "        <category android:name=\"android.intent.category.DEFAULT\" />"
        + "        <category android:name=\"android.intent.category.LAUNCHER\" />"
        + "      </intent-filter>"
        + "    </activity>"
        + "  </application>"
        + "</manifest>";

    @Parameters
    public static Collection<Platform[]> data() {
        List<Platform[]> data = new ArrayList<>();

        String envSkipTest = System.getenv("DM_BOB_BUNDLERTEST_ONLY_HOST");
        String skipTest =  envSkipTest != null ? envSkipTest : System.getProperty("DM_BOB_BUNDLERTEST_ONLY_HOST");
        // By default property is `${DM_BOB_BUNDLERTEST_ONLY_HOST}`
        if (skipTest != null && !skipTest.isEmpty() && !skipTest.equals("0") && !skipTest.equals("${DM_BOB_BUNDLERTEST_ONLY_HOST}")) {
            System.out.println("Bundle test only for host");
            data.add(new Platform[]{Platform.getHostPlatform()});
        }
        else {
            data.add(new Platform[]{Platform.X86Win32});
            data.add(new Platform[]{Platform.X86_64Win32});
            data.add(new Platform[]{Platform.X86_64MacOS});
            data.add(new Platform[]{Platform.Arm64MacOS});
            data.add(new Platform[]{Platform.X86_64Linux});
            data.add(new Platform[]{Platform.Armv7Android});
            data.add(new Platform[]{Platform.JsWeb});

            // Can only do this on OSX machines currently
            if (Platform.getHostPlatform().isMacOS()) {
                data.add(new Platform[]{Platform.Arm64Ios});
                data.add(new Platform[]{Platform.X86_64Ios});
            }
        }
        return data;
    }

    private File getOutputDirFile(String outputDir, String projectName) {
        String folderName = projectName;
        if (platform == Platform.Arm64MacOS || platform == Platform.X86_64MacOS ||
            platform == Platform.Arm64Ios || platform == Platform.X86_64Ios)
        {
            folderName = projectName + ".app";
        }
        return new File(outputDir, folderName);
    }

    private String getBundleAppFolder(String projectName) {
        if (platform == Platform.Arm64Ios || platform == Platform.X86_64Ios)
        {
                return String.format("Payload/%s.app/", projectName);
        }
        return "";
    }

    private void listDir(File dir)
    {
        for (File file : FileUtils.listFiles(dir, new RegexFileFilter(".*"), DirectoryFileFilter.DIRECTORY)) {
            String relative = dir.toURI().relativize(file.toURI()).getPath();
            System.out.printf("%s\n", file);
        }
    }

    private void checkFileExist(File bundleDir, File file)
    {
        if (!file.exists())
        {
            System.out.printf("A missing file %s\n", file);
            System.out.printf("Directory contents:\n");
            listDir(bundleDir);
        }
        assertTrue(file.exists());
    }

    // Used to check if the built and bundled test projects all contain the correct engine binaries.
    private void verifyEngineBinaries() throws IOException
    {
        String projectName = "unnamed";
        String exeName = BundleHelper.projectNameToBinaryName(projectName);
        File outputDirFile = getOutputDirFile(outputDir, projectName);
        assertTrue(outputDirFile.exists());

        if (platform == Platform.X86Win32 || platform == Platform.X86_64Win32)
        {
            File outputBinary = new File(outputDirFile, projectName + ".exe");
            checkFileExist(outputDirFile, outputBinary);
        }
        else if (platform == Platform.Armv7Android)
        {
            File outputApk = new File(outputDirFile, projectName + ".apk");
            checkFileExist(outputDirFile, outputApk);
            FileSystem apkZip = FileSystems.newFileSystem(outputApk.toPath(), new HashMap<>());
            Path enginePathArmv7 = apkZip.getPath("lib/armeabi-v7a/lib" + exeName + ".so");
            assertTrue(Files.isReadable(enginePathArmv7));
            Path classesDexPath = apkZip.getPath("classes.dex");
            assertTrue(Files.isReadable(classesDexPath));
        }
        else if (platform == Platform.Arm64Android)
        {
            File outputApk = new File(outputDirFile, projectName + ".apk");
            checkFileExist(outputDirFile, outputApk);
            FileSystem apkZip = FileSystems.newFileSystem(outputApk.toPath(), new HashMap<>());
            Path enginePathArm64 = apkZip.getPath("lib/arm64-v8a/lib" + exeName + ".so");
            assertTrue(Files.isReadable(enginePathArm64));
            Path classesDexPath = apkZip.getPath("classes.dex");
            assertTrue(Files.isReadable(classesDexPath));
        }
        else if (platform == Platform.WasmWeb)
        {
            File wasmjsFile = new File(outputDirFile, exeName + "_wasm.js");
            checkFileExist(outputDirFile, wasmjsFile);
            File wasmFile = new File(outputDirFile, exeName + ".wasm");
            checkFileExist(outputDirFile, wasmFile);
        }
        else if (platform == Platform.WasmPthreadWeb)
        {
            File wasmjsFile = new File(outputDirFile, exeName + "_pthread_wasm.js");
            checkFileExist(outputDirFile, wasmjsFile);
            File wasmFile = new File(outputDirFile, exeName + "_pthread.wasm");
            checkFileExist(outputDirFile, wasmFile);
        }
        else if (platform == Platform.JsWeb)
        {
            File asmjsFile = new File(outputDirFile, exeName + "_asmjs.js");
            assertTrue(asmjsFile.exists());
        }
        else if (platform == Platform.Arm64Ios || platform == Platform.X86_64Ios)
        {
            List<String> names = Arrays.asList(
                exeName,
                "Info.plist",
                "AppIcon60x60@2x.png",
                "AppIcon60x60@3x.png",
                "AppIcon76x76@2x~ipad.png",
                "AppIcon83.5x83.5@2x~ipad.png",
                "AppIcon76x76~ipad.png"
            );
            for (String name : names) {
                File file = new File(outputDirFile, name);
                checkFileExist(outputDirFile, file);
            }
        }
        else if (platform == Platform.Arm64MacOS || platform == Platform.X86_64MacOS)
        {
            List<String> names = Arrays.asList(
                String.format("Contents/MacOS/%s", exeName),
                "Contents/Info.plist"
            );
            for (String name : names) {
                File file = new File(outputDirFile, name);
                checkFileExist(outputDirFile, file);
            }
        }
        else if (platform == Platform.Arm64Linux || platform == Platform.X86_64Linux)
        {
        }
        else
        {
            throw new IOException("Verifying engine binaries not implemented for platform: " + platform.toString());
        }
    }


    private void verifyArchive() throws IOException
    {
        String projectName = "unnamed";
        File outputDirFile = getOutputDirFile(outputDir, projectName);
        assertTrue(outputDirFile.exists());

        if (platform == Platform.Arm64Android || platform == Platform.Armv7Android)
        {
            File outputApk = new File(outputDirFile, projectName + ".apk");
            assertTrue(outputApk.exists());
            ZipFile apkZip = new ZipFile(outputApk.getAbsolutePath());
            ZipEntry zipEntry = apkZip.getEntry("assets/game.arcd");
            assertFalse(zipEntry == null);
            assertEquals(zipEntry.getMethod(), ZipEntry.STORED);
        }
    }

    private List<String> getZipFiles(File zipFile) throws IOException {
        List<String> files = new ArrayList<String>();
        InputStream inputStream = new FileInputStream(zipFile);
        try (ZipInputStream zipInputStream = new ZipInputStream(inputStream)) {
            ZipEntry zipEntry = zipInputStream.getNextEntry();

            while (zipEntry != null) {
                if (!zipEntry.isDirectory()) {
                    files.add(zipEntry.getName());
                }

                zipInputStream.closeEntry();
                zipEntry = zipInputStream.getNextEntry();
            }
        }
        return files;
    }

    private List<String> getBundleFiles() throws IOException {
        String projectName = "unnamed";
        File outputDirFile = getOutputDirFile(outputDir, projectName);
        assertTrue(outputDirFile.exists());

        List<String> files = new ArrayList<String>();

        if (platform == Platform.Armv7Android || platform == Platform.Arm64Android)
        {
            File zip = new File(outputDirFile, projectName + ".apk");
            assertTrue(zip.exists());
            files = getZipFiles(zip);
        }
        else if (platform == Platform.Arm64Ios || platform == Platform.X86_64Ios)
        {
            File zip = new File(outputDirFile.getParentFile(), projectName + ".ipa");
            assertTrue(zip.exists());
            files = getZipFiles(zip);
        }
        else {
            for (File file : FileUtils.listFiles(outputDirFile, new RegexFileFilter(".*"), DirectoryFileFilter.DIRECTORY)) {
                String relative = outputDirFile.toURI().relativize(file.toURI()).getPath();
                files.add(relative);
            }
        }

        return files;
    }

    public BundlerTest(Platform platform) {
        this.platform = platform;
    }

    @Before
    public void setUp() throws Exception {
        contentRoot = Files.createTempDirectory("defoldtest").toFile().getAbsolutePath();
        outputDir = Files.createTempDirectory("defoldtest").toFile().getAbsolutePath();
        createFile(contentRoot, "game.project", "[display]\nwidth=640\nheight=480\n");

        contentRootUnused = Files.createTempDirectory("defoldtest").toFile().getAbsolutePath();
        createFile(contentRootUnused, "game.project", "[display]\nwidth=640\nheight=480\n[bootstrap]\nmain_collection = /main.collectionc\n");

        BufferedImage image = new BufferedImage(16, 16, BufferedImage.TYPE_4BYTE_ABGR);
        ImageIO.write(image, "png", new File(contentRoot, "test.png"));
    }

    @After
    public void tearDown() throws IOException {
        FileUtils.deleteDirectory(new File(contentRoot));
        FileUtils.deleteDirectory(new File(outputDir));
        FileUtils.deleteDirectory(new File(contentRootUnused));
    }

    void build() throws IOException, CompileExceptionError, MultipleCompileException {
        Project project = new Project(new DefaultFileSystem(), contentRoot, "build");
        project.setPublisher(new NullPublisher(new PublisherSettings()));

        ClassLoaderScanner scanner = new ClassLoaderScanner();
        project.scan(scanner, "com.dynamo.bob");
        project.scan(scanner, "com.dynamo.bob.pipeline");

        setProjectProperties(project);

        List<TaskResult> result = project.build(new NullProgress(), "clean", "build", "bundle");
        for (TaskResult taskResult : result) {
            assertTrue(taskResult.toString(), taskResult.isOk());
        }

        verifyEngineBinaries();
    }

    @SuppressWarnings("unused")
    Set<byte[]> readDarcEntries(String root) throws IOException
    {
        // Read the path entries in the resulting archive
        RandomAccessFile archiveIndex = new RandomAccessFile(root + "/build/game.arci", "r");
        archiveIndex.readInt();  // Version
        archiveIndex.readInt();  // Pad
        archiveIndex.readLong(); // Userdata
        int entryCount  = archiveIndex.readInt();
        int entryOffset = archiveIndex.readInt();
        int hashOffset  = archiveIndex.readInt();
        int hashLength  = archiveIndex.readInt();

        long fileSize = archiveIndex.length();

        Set<byte[]> entries = new HashSet<byte[]>();
        for (int i = 0; i < entryCount; ++i) {
            int offset = hashOffset + (i * ArchiveBuilder.HASH_MAX_LENGTH);
            archiveIndex.seek(offset);
            byte[] buffer = new byte[ArchiveBuilder.HASH_MAX_LENGTH];

            for (int n = 0; n < buffer.length; ++n) {
                buffer[n] = archiveIndex.readByte();
            }

            entries.add(buffer);
        }

        archiveIndex.close();
        return entries;
    }

    // Returns the number of files that will be put into the DARC file
    // Note that the game.project isn't put in the archive either
    protected int createDefaultFiles(String outputContentRoot) throws IOException {
        if (outputContentRoot == null) {
            outputContentRoot = contentRoot;
        }

        int count = 0;
        createFile(outputContentRoot, "logic/main.collection", "name: \"default\"\nscale_along_z: 0\n");
        count++;
        createFile(outputContentRoot, "builtins/render/default.render", "script: \"/builtins/render/default.render_script\"\n");
        count++;
        createFile(outputContentRoot, "builtins/render/default.render_script", "");
        count++;
        createFile(outputContentRoot, "builtins/render/default.display_profiles", " profiles { name: \"Landscape\" qualifiers { width: 1280 height: 720 } } profiles { name: \"Portrait\" qualifiers { width: 720 height: 1280 } } ");
        count++;
        createFile(outputContentRoot, "builtins/input/default.gamepads", "");
        count++;
        createFile(outputContentRoot, "input/game.input_binding", "key_trigger { input: KEY_SPACE action: \"\" }");
        count++;

        // These aren't put in the DARC file, so we don't count up
        createFile(outputContentRoot, "builtins/graphics/default.texture_profiles", "path_settings { path: \"**\" profile: \"Default\"}profiles { name: \"Default\" platforms { os: OS_ID_GENERIC formats { format: TEXTURE_FORMAT_RGBA compressor: \"Uncompressed\" compressor_preset: \"UNCOMPRESSED\" } mipmaps: true }}");
        createFile(outputContentRoot, "builtins/manifests/web/engine_template.html", "");
        createFile(outputContentRoot, "builtins/manifests/web/light_theme.css", "");
        createFile(outputContentRoot, "builtins/manifests/web/dark_theme.css", "");
        createFile(outputContentRoot, "builtins/manifests/osx/Info.plist", "");
        createFile(outputContentRoot, "builtins/manifests/ios/Info.plist", "");
        createFile(outputContentRoot, "builtins/manifests/ios/LaunchScreen.storyboardc/Info.plist", "");
        createFile(outputContentRoot, "builtins/manifests/android/AndroidManifest.xml", ANDROID_MANIFEST);
        createFile(outputContentRoot, "builtins/manifests/web/engine_template.html", "{{{DEFOLD_CUSTOM_CSS_INLINE}}} {{DEFOLD_APP_TITLE}} {{DEFOLD_DISPLAY_WIDTH}} {{DEFOLD_DISPLAY_WIDTH}} {{DEFOLD_ARCHIVE_LOCATION_PREFIX}} {{#HAS_DEFOLD_ENGINE_ARGUMENTS}} {{DEFOLD_ENGINE_ARGUMENTS}} {{/HAS_DEFOLD_ENGINE_ARGUMENTS}} {{DEFOLD_SPLASH_IMAGE}} {{DEFOLD_HEAP_SIZE}} {{DEFOLD_BINARY_PREFIX}} {{DEFOLD_BINARY_PREFIX}} {{DEFOLD_BINARY_PREFIX}} {{DEFOLD_HAS_FACEBOOK_APP_ID}}");
        return count;
    }

    @Test
    public void testBundle() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException {
        createDefaultFiles(contentRoot);
        createFile(contentRoot, "test.icns", "test_icon");
        build();
        verifyArchive();
    }

    @Test
    public void testBundleOutputInsideBuildDirectoryRejected() throws IOException, ConfigurationException, MultipleCompileException {
        createDefaultFiles(contentRoot);
        outputDir = new File(contentRoot, "build").getAbsolutePath();

        Project project = new Project(new DefaultFileSystem(), contentRoot, "build");
        project.setPublisher(new NullPublisher(new PublisherSettings()));

        ClassLoaderScanner scanner = new ClassLoaderScanner();
        project.scan(scanner, "com.dynamo.bob");
        project.scan(scanner, "com.dynamo.bob.pipeline");

        setProjectProperties(project);

        try {
            project.build(new NullProgress(), "bundle");
            fail("Expected bundle output under build directory to be rejected.");
        } catch (CompileExceptionError e) {
            assertTrue(e.getMessage().contains(outputDir));
        }
    }

    private String createFile(String root, String name, String content) throws IOException {
        File file = new File(root, name);
        FileUtil.deleteOnExit(file);
        FileUtils.copyInputStreamToFile(new ByteArrayInputStream(content.getBytes()), file);
        return file.getAbsolutePath();
    }

    private void setProjectProperties(Project project) {
        Platform buildPlatform = platform;
        if (platform == Platform.Armv7Android || platform == Platform.Arm64Android) {
            buildPlatform = Platform.Armv7Android;
        }
        else if (platform == Platform.Arm64Ios || platform == Platform.X86_64Ios) {
            buildPlatform = Platform.Arm64Ios;
        }

        project.setOption("platform", buildPlatform.getPair());
        project.setOption("architectures", platform.getPair());
        project.setOption("archive", "true");
        project.setOption("bundle-output", outputDir);
    }

    @Test
    public void testCustomResourcesFile() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException {
        int numBuiltins = createDefaultFiles(contentRoot);
        createFile(contentRoot, "game.project", "[project]\ncustom_resources=m.txt\n[display]\nwidth=640\nheight=480\n");
        createFile(contentRoot, "m.txt", "dummy");
        build();

        Set<byte[]> entries = readDarcEntries(contentRoot);
        assertEquals(1, entries.size() - numBuiltins);
    }

    @Test
    public void testCustomResourcesDirs() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException {
        File cust = new File(contentRoot, "custom");
        cust.mkdir();
        File sub1 = new File(cust, "sub1");
        File sub2 = new File(cust, "sub2");
        sub1.mkdir();
        sub2.mkdir();
        int numBuiltins = createDefaultFiles(contentRoot);
        createFile(contentRoot, "m.txt", "dummy");
        createFile(sub1.getAbsolutePath(), "s1-1.txt", "dummy1");
        createFile(sub1.getAbsolutePath(), "s1-2.txt", "dummy2");
        createFile(sub2.getAbsolutePath(), "s2-1.txt", "dummy3");
        createFile(sub2.getAbsolutePath(), "s2-2.txt", "dummy4");

        createFile(contentRoot, "game.project", "[project]\ncustom_resources=custom,m.txt\n[display]\nwidth=640\nheight=480\n");
        build();
        Set<byte[]> entries = readDarcEntries(contentRoot);
        assertEquals(5, entries.size() - numBuiltins);

        createFile(contentRoot, "game.project", "[project]\ncustom_resources=custom/sub2\n[display]\nwidth=640\nheight=480\n");
        build();
        entries = readDarcEntries(contentRoot);
        assertEquals(2, entries.size() - numBuiltins);
    }

    // Historically it has been possible to include custom resources by both specifying project relative paths and absolute paths.
    // (The only difference being a leading slash.) To keep backwards compatibility we need to support both.
    @Test
    public void testAbsoluteCustomResourcePath() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException, NoSuchAlgorithmException {
        final String expectedData = "dummy";
        final HashAlgorithm hashAlgo = HashAlgorithm.HASH_SHA1;
        final byte[] expectedHash = ManifestBuilder.CryptographicOperations.hash(expectedData.getBytes(), hashAlgo);
        final int hlen = ManifestBuilder.CryptographicOperations.getHashSize(hashAlgo);
        int numBuiltins = createDefaultFiles(contentRoot);
        createFile(contentRoot, "game.project", "[project]\ncustom_resources=/m.txt\n[display]\nwidth=640\nheight=480\n");
        createFile(contentRoot, "m.txt", expectedData);
        build();

        Set<byte[]> entries = readDarcEntries(contentRoot);
        assertEquals(1, entries.size() - numBuiltins);

        // Verify that the entry contained in the darc has the same hash as m.txt
        boolean found = false;
        for (byte[] b : entries) {
            boolean ok = true;
            for (int i = 0; i < hlen; ++i) {
                if (expectedHash[i] != b[i]) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                found = true;
                break;
            }
        }
        assertTrue(found);
    }

    static HashSet<String> getExpectedFilesForPlatform(Platform platform, HashSet<String> actualFiles)
    {
        HashSet<String> expectedFiles = new HashSet<String>();
        if (platform == Platform.X86Win32 || platform == Platform.X86_64Win32)
        {
                expectedFiles.add("unnamed.exe");
                expectedFiles.add("game.public.der");
                expectedFiles.add("game.dmanifest");
                expectedFiles.add("game.arci");
                expectedFiles.add("game.arcd");
                expectedFiles.add("game.projectc");
        }
        else if (platform == Platform.WasmWeb)
        {
                expectedFiles.add("dmloader.js");
                expectedFiles.add("index.html");
                expectedFiles.add("unnamed_wasm.js");
                expectedFiles.add("unnamed.wasm");
                expectedFiles.add("archive/game0.arcd");
                expectedFiles.add("archive/game0.arci");
                expectedFiles.add("archive/game0.dmanifest");
                expectedFiles.add("archive/game0.projectc");
                expectedFiles.add("archive/game0.public.der");
                expectedFiles.add("archive/archive_files.json");
        }
        else if (platform == Platform.JsWeb)
        {
                expectedFiles.add("dmloader.js");
                expectedFiles.add("index.html");
                expectedFiles.add("unnamed_asmjs.js");
                expectedFiles.add("archive/game0.arcd");
                expectedFiles.add("archive/game0.arci");
                expectedFiles.add("archive/game0.dmanifest");
                expectedFiles.add("archive/game0.projectc");
                expectedFiles.add("archive/game0.public.der");
                expectedFiles.add("archive/archive_files.json");
        }
        else if (platform == Platform.Armv7Android || platform == Platform.Arm64Android)
        {
            expectedFiles.add("classes.dex");
            expectedFiles.add("lib/armeabi-v7a/libunnamed.so");
            expectedFiles.add("AndroidManifest.xml");
            expectedFiles.add("assets/game.arcd");
            expectedFiles.add("assets/game.arci");
            expectedFiles.add("assets/game.dmanifest");
            expectedFiles.add("assets/game.projectc");
            expectedFiles.add("assets/game.public.der");
            expectedFiles.add("META-INF/MANIFEST.MF");
            expectedFiles.add("res/drawable-hdpi-v4/icon.png");
            expectedFiles.add("res/drawable-ldpi-v4/icon.png");
            expectedFiles.add("res/drawable-mdpi-v4/icon.png");
            expectedFiles.add("res/drawable-xhdpi-v4/icon.png");
            expectedFiles.add("res/drawable-xxhdpi-v4/icon.png");
            expectedFiles.add("res/drawable-xxxhdpi-v4/icon.png");
            expectedFiles.add("res/xml/splits0.xml");
            expectedFiles.add("resources.arsc");
            // the APK may or may not be signed, depending on if bundletool was able
            // to find a debug keystore on the system or not
            if (actualFiles.contains("META-INF/BNDLTOOL.SF")) {
                expectedFiles.add("META-INF/BNDLTOOL.SF");
            }
            if (actualFiles.contains("META-INF/BNDLTOOL.RSA")) {
                expectedFiles.add("META-INF/BNDLTOOL.RSA");
            }
        }
        else if (platform == Platform.Arm64Ios || platform == Platform.X86_64Ios)
        {
            expectedFiles.add("Payload/unnamed.app/unnamed");
            expectedFiles.add("Payload/unnamed.app/Info.plist");
            expectedFiles.add("Payload/unnamed.app/LaunchScreen.storyboardc/Info.plist");
            expectedFiles.add("Payload/unnamed.app/game.arcd");
            expectedFiles.add("Payload/unnamed.app/game.arci");
            expectedFiles.add("Payload/unnamed.app/game.dmanifest");
            expectedFiles.add("Payload/unnamed.app/game.projectc");
            expectedFiles.add("Payload/unnamed.app/game.public.der");
            expectedFiles.add("Payload/unnamed.app/AppIcon60x60@2x.png");
            expectedFiles.add("Payload/unnamed.app/AppIcon60x60@3x.png");
            expectedFiles.add("Payload/unnamed.app/AppIcon76x76@2x~ipad.png");
            expectedFiles.add("Payload/unnamed.app/AppIcon83.5x83.5@2x~ipad.png");
            expectedFiles.add("Payload/unnamed.app/AppIcon76x76~ipad.png");
        }
        else if (platform == Platform.X86_64MacOS || platform == Platform.Arm64MacOS)
        {
            expectedFiles.add("Contents/MacOS/unnamed");
            expectedFiles.add("Contents/Info.plist");
            expectedFiles.add("Contents/Resources/game.arcd");
            expectedFiles.add("Contents/Resources/game.arci");
            expectedFiles.add("Contents/Resources/game.dmanifest");
            expectedFiles.add("Contents/Resources/game.projectc");
            expectedFiles.add("Contents/Resources/game.public.der");
        }
        else if (platform == Platform.X86_64Linux)
        {
            expectedFiles.add("unnamed.x86_64");
            expectedFiles.add("game.arcd");
            expectedFiles.add("game.arci");
            expectedFiles.add("game.dmanifest");
            expectedFiles.add("game.projectc");
            expectedFiles.add("game.public.der");
        }
        else if (platform == Platform.Arm64Linux)
        {
            expectedFiles.add("unnamed.arm64");
            expectedFiles.add("game.arcd");
            expectedFiles.add("game.arci");
            expectedFiles.add("game.dmanifest");
            expectedFiles.add("game.projectc");
            expectedFiles.add("game.public.der");
        }
        else
        {
            System.err.println("Expected file set is not implemented for this platform.");
        }

        return expectedFiles;
    }

    private void dumpExpectedAndActualFiles(Set<String> expectedFiles, Set<String> actualFiles) {
        if (expectedFiles.size() != actualFiles.size()) {
            for (String file : actualFiles) {
                System.out.println("Actual file:" + file);
            }
            for (String file : expectedFiles) {
                System.out.println("Expected file:" + file);
            }
        }
    }

    @Test
    public void testBundleResourcesDirs() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException {
        System.out.println("Platform:" + platform.toString());
        final boolean isAndroid = (platform == Platform.Armv7Android || platform == Platform.Arm64Android);
        final String appFolder = getBundleAppFolder("unnamed");
        /*
         * Project structure:
         *
         *  /
         *  +-/m.txt
         *  +-<built-ins> (from createDefaultFiles)
         *  +-custom/
         *  | EMPTY!
         *  +-sub1/
         *    +-common/
         *      +-s1-root1.txt
         *      +-s1-root2.txt
         *  +-sub2/
         *    +-[current-platform-arch]/
         *      +-s2-root1.txt
         *      +-s2-root2.txt
         *      |
         *    The folders below only on Android
         *      |
         *      +-assets/
         *        +-s2-asset.txt
         *      +-lib/
         *        +-[current-platform-arch]/
         *          +-lib.so
         *      +-res/
         *        +-raw/
         *          +-s2_raw.txt               <-- must contain only [a-z0-9_.]
         *        +-values/
         *          +-s2-values.xml
         */
        File cust = new File(contentRoot, "custom");
        cust.mkdir();
        File sub1 = new File(contentRoot, "sub1");
        File sub2 = new File(contentRoot, "sub2");
        sub1.mkdir();
        sub2.mkdir();
        File sub_platform1 = new File(sub1, "common"); // common is a platform
        File sub_platform2 = new File(sub2, this.platform.getExtenderPair());
        sub_platform1.mkdir();
        sub_platform2.mkdir();
        createDefaultFiles(contentRoot);
        createFile(contentRoot, "m.txt", "dummy");
        createFile(sub_platform1.getAbsolutePath(), "s1-root1.txt", "dummy");
        createFile(sub_platform1.getAbsolutePath(), "s1-root2.txt", "dummy");
        createFile(sub_platform2.getAbsolutePath(), "s2-root1.txt", "dummy");
        createFile(sub_platform2.getAbsolutePath(), "s2-root2.txt", "dummy");

        // some additional files and folders which will only be included on Android
        if (isAndroid) {
            // res and subfolders
            File sub_platform2_res = new File(sub_platform2, "res");
            File sub_platform2_res_raw = new File(sub_platform2_res, "raw");
            File sub_platform2_res_values = new File(sub_platform2_res, "values");
            sub_platform2_res.mkdir();
            sub_platform2_res_raw.mkdir();
            sub_platform2_res_values.mkdir();
            // this is a "raw" file and it will be included as-is in the res folder of the APK
            createFile(sub_platform2_res_raw.getAbsolutePath(), "s2_raw.txt", "dummy");
            // this is a resource file and it will get included in the resources.arsc file
            createFile(sub_platform2_res_values.getAbsolutePath(), "s2-values.xml", "<?xml version=\"1.0\" encoding=\"utf-8\"?><resources></resources>");

            // assets will be included in the asset folder of the APK
            File sub_platform2_assets = new File(sub_platform2, "assets");
            sub_platform2_assets.mkdir();
            createFile(sub_platform2_assets.getAbsolutePath(), "s2-asset.txt", "dummy");

            // libs will be included in the lib/architecture folder of the APK
            File sub_platform2_lib = new File(sub_platform2, "lib");
            File sub_platform2_lib_arch = new File(sub_platform2_lib, (platform == Platform.Armv7Android) ? "armeabi-v7a" : "arm64-v8a");
            sub_platform2_lib.mkdir();
            sub_platform2_lib_arch.mkdir();
            createFile(sub_platform2_lib_arch.getAbsolutePath(), "s2-lib.so", "dummy");
        }


        // first test - no bundle resources
        createFile(contentRoot, "game.project", "[project]\nbundle_resources=\n[display]\nwidth=640\nheight=480\n");
        build();
        HashSet<String> actualFiles = new HashSet<String>(getBundleFiles());
        HashSet<String> expectedFiles = getExpectedFilesForPlatform(platform, actualFiles);
        dumpExpectedAndActualFiles(expectedFiles, actualFiles);
        assertEquals(expectedFiles.size(), actualFiles.size());
        assertEquals(expectedFiles, actualFiles);


        // second test - /sub1
        createFile(contentRoot, "game.project", "[project]\nbundle_resources=/sub1\n[display]\nwidth=640\nheight=480\n");
        build();
        actualFiles = new HashSet<String>(getBundleFiles());
        expectedFiles = getExpectedFilesForPlatform(platform, actualFiles);
        expectedFiles.add(appFolder + "s1-root1.txt");
        expectedFiles.add(appFolder + "s1-root2.txt");
        dumpExpectedAndActualFiles(expectedFiles, actualFiles);
        assertEquals(expectedFiles.size(), actualFiles.size());
        assertEquals(expectedFiles, actualFiles);


        // third test - /sub1 and /sub2
        createFile(contentRoot, "game.project", "[project]\nbundle_resources=/sub1,/sub2\n[display]\nwidth=640\nheight=480\n");
        build();
        actualFiles = new HashSet<String>(getBundleFiles());
        expectedFiles = getExpectedFilesForPlatform(platform, actualFiles);
        expectedFiles.add(appFolder + "s1-root1.txt");
        expectedFiles.add(appFolder + "s1-root2.txt");
        expectedFiles.add(appFolder + "s2-root1.txt");
        expectedFiles.add(appFolder + "s2-root2.txt");
        if (isAndroid) expectedFiles.add("res/raw/s2_raw.txt");
        if (isAndroid) expectedFiles.add("assets/s2-asset.txt");
        if (isAndroid) expectedFiles.add("lib/" + ((platform == Platform.Armv7Android) ? "armeabi-v7a" : "arm64-v8a") + "/s2-lib.so");
        dumpExpectedAndActualFiles(expectedFiles, actualFiles);
        assertEquals(expectedFiles.size(), actualFiles.size());
        assertEquals(expectedFiles, actualFiles);
    }

    @Test
    public void testBundleWithDynamicLibraries()
            throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException {
        if (platform == Platform.JsWeb || platform == Platform.WasmWeb) {
            return;
        }

        createDefaultFiles(contentRoot);

        Project project = new Project(new DefaultFileSystem(), contentRoot, "build");
        project.setPublisher(new NullPublisher(new PublisherSettings()));
        ClassLoaderScanner scanner = new ClassLoaderScanner();
        project.scan(scanner, "com.dynamo.bob");
        project.scan(scanner, "com.dynamo.bob.pipeline");
        setProjectProperties(project);

        List<TaskResult> buildResult = project.build(new NullProgress(), "clean", "build");
        for (TaskResult taskResult : buildResult) {
            assertTrue(taskResult.toString(), taskResult.isOk());
        }

        String binaryOutputDir = project.getBinaryOutputDirectory();
        File platformBinaryDir = new File(binaryOutputDir, platform.getExtenderPair());
        platformBinaryDir.mkdirs();

        String libName = platform.getLibPrefix() + "testlib" + platform.getLibSuffix();
        createFile(platformBinaryDir.getAbsolutePath(), libName, "mock_library_content");

        List<TaskResult> bundleResult = project.build(new NullProgress(), "bundle");
        for (TaskResult taskResult : bundleResult) {
            assertTrue(taskResult.toString(), taskResult.isOk());
        }

        List<String> bundleFiles = getBundleFiles();
        String expectedLibPath = getExpectedDynamicLibraryPath(libName);

        if (expectedLibPath != null) {
            assertTrue(
                    "Expected dynamic library " + expectedLibPath + " not found in bundle. Bundle files: "
                            + bundleFiles,
                    bundleFiles.contains(expectedLibPath));
        }
    }

    private String getExpectedDynamicLibraryPath(String libName) {
        if (platform == Platform.X86_64Linux || platform == Platform.Arm64Linux) {
            return libName;
        } else if (platform == Platform.X86Win32 || platform == Platform.X86_64Win32) {
            return libName;
        } else if (platform == Platform.X86_64MacOS || platform == Platform.Arm64MacOS) {
            return "Contents/MacOS/" + libName;
        } else if (platform == Platform.Arm64Ios || platform == Platform.X86_64Ios) {
            return "Payload/unnamed.app/" + libName;
        } else if (platform == Platform.Armv7Android) {
            return "lib/armeabi-v7a/" + libName;
        } else if (platform == Platform.Arm64Android) {
            return "lib/arm64-v8a/" + libName;
        }
        return null;
    }
}
