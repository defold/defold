package com.dynamo.bob.bundle.test;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.apache.commons.io.FileUtils;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;

import com.dynamo.bob.Platform;
import com.dynamo.bob.bundle.ManifestMergeTool;

@RunWith(Parameterized.class)
public class ManifestMergeToolTest {

    private String contentRoot;
    private Platform platform;
    private File root;
    private File main;
    private File target;
    List<File> libraries;

    private String createFile(String root, String name, String content) throws IOException {
        File file = new File(root, name);
        file.deleteOnExit();
        FileUtils.copyInputStreamToFile(new ByteArrayInputStream(content.getBytes()), file);
        return file.getAbsolutePath();
    }

    protected void createDefaultFiles() throws IOException {
        // These are the base manifest files
        String iosManifest = ""
                + "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                + "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                + "<plist version=\"1.0\">\n"
                + "<dict>\n"
                + "        <key>NSAppTransportSecurity</key>\n"
                + "        <dict>\n"
                + "            <key>NSExceptionDomains</key>\n"
                + "            <dict>\n"
                + "                <key>foobar.net</key>\n"
                + "                <dict>\n"
                + "                    <key>testproperty</key>\n"
                + "                    <true/>\n"
                + "                </dict>\n"
                + "            </dict>\n"
                + "        </dict>\n"
                + "        <key>INT</key>\n"
                + "        <integer>8</integer>\n"
                + "        <key>REAL</key>\n"
                + "        <real>8.0</real>\n"
                + "        <key>BASE64</key>\n"
                + "        <data>SEVMTE8gV09STEQ=</data>\n"
                + "</dict>\n"
                + "</plist>\n";

        createFile(contentRoot, "builtins/manifests/ios/Info.plist", iosManifest);

        String androidManifest = ""
                + "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                + "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\""
                + "        package=\"com.defold.testmerge\""
                + "        android:versionCode=\"14\""
                + "        android:versionName=\"1.0\""
                + "        android:installLocation=\"auto\">"
                + "    <uses-feature android:required=\"true\" android:glEsVersion=\"0x00020000\" />"
                + "    <uses-sdk android:minSdkVersion=\"9\" android:targetSdkVersion=\"26\" />"
                + "    <application android:label=\"Test Project\" android:hasCode=\"true\">"
                + "    </application>"
                + "    <uses-permission android:name=\"android.permission.VIBRATE\" />"
                + "</manifest>";

        createFile(contentRoot, "builtins/manifests/android/AndroidManifest.xml", androidManifest);

        if (platform == Platform.Armv7Android) {
            this.target = new File(contentRoot, "builtins/manifests/android/AndroidManifestMerged.xml");
            this.main = new File(contentRoot, "builtins/manifests/android/AndroidManifest.xml");
            this.libraries = new ArrayList<File>();
            this.libraries.add(new File(contentRoot, "builtins/manifests/android/AndroidManifestLib.xml"));
        } else if (platform == Platform.Armv7Darwin) {
            this.target = new File(contentRoot, "builtins/manifests/ios/InfoMerged.plist");
            this.main = new File(contentRoot, "builtins/manifests/ios/Info.plist");
            this.libraries = new ArrayList<File>();
            this.libraries.add(new File(contentRoot, "builtins/manifests/ios/InfoLib.plist"));
        }
    }

    @Parameters
    public static Collection<Platform[]> data() {
        Platform[][] data = new Platform[][] { {Platform.Armv7Android}, {Platform.Armv7Darwin}};
        return Arrays.asList(data);
    }

    public ManifestMergeToolTest(Platform platform) throws IOException {
        this.platform = platform;
        root = Files.createTempDirectory("defoldmergetest").toFile();
        root.deleteOnExit();
        contentRoot = root.getAbsolutePath();
    }

    private String readFile(File file) throws IOException {
        List<String> lines = Files.readAllLines(file.toPath());

        String s = "";
        for (String line : lines) {
            s += line + "\n";
        }
        return s;
    }

    @Test
    public void testMergeAndroid() throws IOException {
        if (platform != Platform.Armv7Android) {
            return;
        }

        createDefaultFiles();

        String manifest = ""
                + "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                + "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" package=\"com.defold.testmerge\">"
                + "    <uses-feature android:required=\"true\" android:glEsVersion=\"0x00030000\" />"
                + "    <application>"
                + "        <meta-data android:name=\"com.facebook.sdk.ApplicationName\""
                + "            android:value=\"Test Project\" />"
                + "        <activity android:name=\"com.facebook.FacebookActivity\""
                + "          android:theme=\"@android:style/Theme.Translucent.NoTitleBar\""
                + "          android:configChanges=\"keyboard|keyboardHidden|screenLayout|screenSize|orientation\""
                + "          android:label=\"Test Project\" />"
                + "    </application>"
                + "</manifest>";
        createFile(contentRoot, "builtins/manifests/android/AndroidManifestLib.xml", manifest);

        String expected = ""
                + "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                + "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"\n"
                + "    package=\"com.defold.testmerge\"\n"
                + "    android:installLocation=\"auto\"\n"
                + "    android:versionCode=\"14\"\n"
                + "    android:versionName=\"1.0\" >\n"
                + "\n"
                + "    <uses-sdk\n"
                + "        android:minSdkVersion=\"9\"\n"
                + "        android:targetSdkVersion=\"26\" />\n"
                + "\n"
                + "    <uses-permission android:name=\"android.permission.VIBRATE\" />\n"
                + "\n"
                + "    <uses-feature\n"
                + "        android:glEsVersion=\"0x00030000\"\n"
                + "        android:required=\"true\" />\n"
                + "\n"
                + "    <uses-permission android:name=\"android.permission.WRITE_EXTERNAL_STORAGE\" />\n"
                + "    <uses-permission android:name=\"android.permission.READ_PHONE_STATE\" />\n"
                + "    <uses-permission android:name=\"android.permission.READ_EXTERNAL_STORAGE\" />\n"
                + "\n"
                + "    <application\n"
                + "        android:hasCode=\"true\"\n"
                + "        android:label=\"Test Project\" >\n"
                + "        <meta-data\n"
                + "            android:name=\"com.facebook.sdk.ApplicationName\"\n"
                + "            android:value=\"Test Project\" />\n"
                + "\n"
                + "        <activity\n"
                + "            android:name=\"com.facebook.FacebookActivity\"\n"
                + "            android:configChanges=\"keyboard|keyboardHidden|screenLayout|screenSize|orientation\"\n"
                + "            android:label=\"Test Project\"\n"
                + "            android:theme=\"@android:style/Theme.Translucent.NoTitleBar\" />\n"
                + "    </application>\n"
                + "\n"
                + "</manifest>\n";


        createFile(contentRoot, "builtins/manifests/android/AndroidManifestExpected.xml", expected);


        ManifestMergeTool.merge(ManifestMergeTool.Platform.ANDROID, this.main, this.target, this.libraries);

        String merged = readFile(target);
        assertEquals(expected, merged);
    }

    @Test
    public void testMergeAndroidFailSdkMinVersion() throws IOException {
        if (platform != Platform.Armv7Android) {
            return;
        }

        createDefaultFiles();

        String manifest = ""
                + "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                + "<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" package=\"com.defold.testmerge\">"
                + "    <uses-sdk android:minSdkVersion=\"14\" android:targetSdkVersion=\"28\" />"
                + "</manifest>";
        createFile(contentRoot, "builtins/manifests/android/AndroidManifestLib.xml", manifest);

        try {
            ManifestMergeTool.merge(ManifestMergeTool.Platform.ANDROID, this.main, this.target, this.libraries);
            assertTrue(false); // We shouldn't get here!
        } catch (RuntimeException e) {
            assertTrue(true);
        }
    }


    @Test
    public void testMergeIOS() throws IOException {
        if (platform != Platform.Armv7Darwin) {
            return;
        }

        createDefaultFiles();

        String libManifest = ""
                + "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                + "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                + "<plist version=\"1.0\">\n"
                + "<dict>\n"
                + "    <key>NSAppTransportSecurity</key>\n"
                + "    <dict>\n"
                + "        <key>NSExceptionDomains</key>\n"
                + "        <dict>\n"
                + "            <key>facebook.com</key>\n"
                + "            <dict>\n"
                + "                <key>NSIncludesSubdomains</key>\n"
                + "                <true/>\n"
                + "                <key>NSThirdPartyExceptionRequiresForwardSecrecy</key>\n"
                + "                <false/>\n"
                + "            </dict>\n"
                + "        </dict>\n"
                + "    </dict>\n"
                + "    <key>INT</key>\n"
                + "    <integer>42</integer>\n"
                + "</dict>\n"
                + "</plist>\n";

        createFile(contentRoot, "builtins/manifests/ios/InfoLib.plist", libManifest);

        String expected = ""
                + "<?xml version=\"1.0\"?>\n"
                + "<!DOCTYPE plist SYSTEM \"file://localhost/System/Library/DTDs/PropertyList.dtd\">\n"
                + "<plist version=\"1.0\">\n"
                + "    <dict>\n"
                + "        <key>NSAppTransportSecurity</key>\n"
                + "        <dict>\n"
                + "            <key>NSExceptionDomains</key>\n"
                + "            <dict>\n"
                + "                <key>foobar.net</key>\n"
                + "                <dict>\n"
                + "                    <key>testproperty</key>\n"
                + "                    <true/>\n"
                + "                </dict>\n"
                + "\n"
                + "                <key>facebook.com</key>\n"
                + "                <dict>\n"
                + "                    <key>NSIncludesSubdomains</key>\n"
                + "                    <true/>\n"
                + "\n"
                + "                    <key>NSThirdPartyExceptionRequiresForwardSecrecy</key>\n"
                + "                    <false/>\n"
                + "                </dict>\n"
                + "            </dict>\n"
                + "        </dict>\n"
                + "\n"
                + "        <key>INT</key>\n"
                + "        <integer>8</integer>\n"
                + "\n"
                + "        <key>REAL</key>\n"
                + "        <real>8.0</real>\n"
                + "\n"
                + "        <key>BASE64</key>\n"
                + "        <data>SEVMTE8gV09STEQ=</data>\n"
                + "\n"
                + "        <key>INT</key>\n"
                + "        <integer>42</integer>\n"
                + "    </dict>\n"
                + "</plist>\n";

        createFile(contentRoot, "builtins/manifests/ios/InfoExpected.plist", expected);


        ManifestMergeTool.merge(ManifestMergeTool.Platform.IOS, this.main, this.target, this.libraries);

        String merged = readFile(target);
        assertEquals(expected, merged);
    }

    @Test
    public void testMergeIOSMixedTypes() throws IOException {
        if (platform != Platform.Armv7Darwin) {
            return;
        }

        createDefaultFiles();

        String libManifest = ""
                + "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                + "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                + "<plist version=\"1.0\">\n"
                + "<dict>\n"
                + "    <key>INT</key>\n"
                + "    <data>42</data>\n"
                + "</dict>\n"
                + "</plist>\n";

        createFile(contentRoot, "builtins/manifests/ios/InfoLib.plist", libManifest);

        try {
            ManifestMergeTool.merge(ManifestMergeTool.Platform.IOS, this.main, this.target, this.libraries);
            assertTrue(false); // We shouldn't get here!
        } catch (RuntimeException e) {
            assertTrue(true);
        }
    }
}
