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

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import org.apache.commons.io.FileUtils;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import com.dynamo.bob.bundle.IOSBundler;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

public class IOSBundlerTest {
    @Rule
    public TemporaryFolder temporaryFolder = new TemporaryFolder();

    private static String plist(String body) {
        return "<?xml version=\"1.0\"?><plist version=\"1.0\"><dict>" + body + "</dict></plist>";
    }

    private static String scene(String configuration) {
        return plist("<key>UIApplicationSceneManifest</key><dict>"
                + "<key>UIApplicationSupportsMultipleScenes</key><false/>"
                + "<key>UISceneConfigurations</key><dict>"
                + "<key>UIWindowSceneSessionRoleApplication</key><array><dict>"
                + configuration + "</dict></array></dict></dict>");
    }

    // Accept both the default static scene declaration and the engine's programmatic configuration.
    @Test
    public void acceptsEngineSceneConfiguration() throws IOException {
        IOSBundler.validateSceneManifest(scene("<key>UISceneDelegateClassName</key><string>DefoldSceneDelegate</string>"));
        IOSBundler.validateSceneManifest(plist("<key>UIApplicationSceneManifest</key><dict><key>UIApplicationSupportsMultipleScenes</key><false/></dict>"));
    }

    // Old custom plists must fail during bundling rather than produce an app without a game scene.
    @Test(expected = IOException.class)
    public void rejectsMissingSceneManifest() throws IOException {
        IOSBundler.validateSceneManifest(plist("<key>CFBundleName</key><string>Game</string>"));
    }

    // Enabling multiple scenes would allow multiple windows to mutate the single engine instance.
    @Test(expected = IOException.class)
    public void rejectsMultipleScenes() throws IOException {
        IOSBundler.validateSceneManifest(plist("<key>UIApplicationSceneManifest</key><dict><key>UIApplicationSupportsMultipleScenes</key><true/></dict>"));
    }

    // Extension-merged plists must not replace the engine's scene delegate.
    @Test(expected = IOException.class)
    public void rejectsCustomSceneDelegate() throws IOException {
        IOSBundler.validateSceneManifest(scene("<key>UISceneDelegateClassName</key><string>OtherSceneDelegate</string>"));
    }

    // A scene storyboard would create a second UI instead of Defold's programmatic game view.
    @Test(expected = IOException.class)
    public void rejectsSceneStoryboard() throws IOException {
        IOSBundler.validateSceneManifest(scene("<key>UISceneStoryboardFile</key><string>Main</string>"));
    }

    private static void write(File file, String content) throws IOException {
        Files.write(file.toPath(), content.getBytes("UTF-8"));
    }

    // Swift runtime libraries added by the build server land in the app root as shared libraries, but only
    // Frameworks is on the engine's rpath and is signed. Other libraries and subfolders must stay where they are.
    @Test
    public void movesSwiftRuntimeLibrariesToFrameworks() throws IOException {
        File appDir = temporaryFolder.newFolder("app");
        File frameworksDir = new File(appDir, "Frameworks");
        assertTrue(frameworksDir.mkdirs());
        write(new File(appDir, "libswift_Concurrency.dylib"), "concurrency");
        write(new File(appDir, "libswiftCompatibility56.dylib"), "compat56");
        write(new File(appDir, "libother.dylib"), "other");
        write(new File(appDir, "libswift_notes.txt"), "notes");
        write(new File(frameworksDir, "libswiftkeep.dylib"), "keep");

        IOSBundler.moveSwiftRuntimeLibraries(appDir, frameworksDir);

        assertFalse(new File(appDir, "libswift_Concurrency.dylib").exists());
        assertFalse(new File(appDir, "libswiftCompatibility56.dylib").exists());
        assertEquals("concurrency", FileUtils.readFileToString(new File(frameworksDir, "libswift_Concurrency.dylib"), "UTF-8"));
        assertEquals("compat56", FileUtils.readFileToString(new File(frameworksDir, "libswiftCompatibility56.dylib"), "UTF-8"));
        assertEquals("keep", FileUtils.readFileToString(new File(frameworksDir, "libswiftkeep.dylib"), "UTF-8"));
        assertTrue(new File(appDir, "libother.dylib").exists());
        assertTrue(new File(appDir, "libswift_notes.txt").exists());
        assertFalse(new File(frameworksDir, "libother.dylib").exists());
    }

    // A library with the same name in Frameworks, for example a copy that an extension ships, must not make the move fail.
    @Test
    public void swiftRuntimeLibraryMoveOverwritesExistingFrameworkCopy() throws IOException {
        File appDir = temporaryFolder.newFolder("app");
        File frameworksDir = new File(appDir, "Frameworks");
        assertTrue(frameworksDir.mkdirs());
        write(new File(appDir, "libswiftCore.dylib"), "new");
        write(new File(frameworksDir, "libswiftCore.dylib"), "old");

        IOSBundler.moveSwiftRuntimeLibraries(appDir, frameworksDir);

        assertFalse(new File(appDir, "libswiftCore.dylib").exists());
        assertEquals("new", FileUtils.readFileToString(new File(frameworksDir, "libswiftCore.dylib"), "UTF-8"));
    }

    // Projects without Swift extensions have no runtime libraries, and the bundling must keep working for them.
    @Test
    public void swiftRuntimeLibraryMoveIgnoresAppsWithoutSwiftLibraries() throws IOException {
        File appDir = temporaryFolder.newFolder("app");
        File frameworksDir = new File(appDir, "Frameworks");
        assertTrue(frameworksDir.mkdirs());

        IOSBundler.moveSwiftRuntimeLibraries(appDir, frameworksDir);
        IOSBundler.moveSwiftRuntimeLibraries(new File(appDir, "missing"), frameworksDir);

        assertEquals(0, frameworksDir.list().length);
    }
}
