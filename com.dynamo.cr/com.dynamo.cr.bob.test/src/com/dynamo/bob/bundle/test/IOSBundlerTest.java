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

import java.io.IOException;
import org.junit.Test;
import com.dynamo.bob.bundle.IOSBundler;

public class IOSBundlerTest {
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
}
