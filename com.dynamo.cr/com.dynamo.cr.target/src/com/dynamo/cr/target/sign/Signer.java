package com.dynamo.cr.target.sign;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.configuration.plist.XMLPropertyListConfiguration;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.cr.common.util.Exec;
import com.dynamo.cr.target.core.TargetPlugin;
import com.google.common.io.Files;

public class Signer {
    private static Logger logger = LoggerFactory.getLogger(Signer.class);

    public String sign(String identity, String provisioningProfile, String exe, Map<String, String> properties, Map<String, IResource> bundleResources) throws IOException, ConfigurationException {
        File packageDir = Files.createTempDir();
        File appDir = new File(packageDir, "Defold.app");
        appDir.mkdirs();

        // Create Info.plist
        InputStream infoIn = getClass().getResourceAsStream("Info.plist");
        XMLPropertyListConfiguration info = new XMLPropertyListConfiguration();
        info.load(infoIn);
        infoIn.close();
        for (Entry<String, String> e : properties.entrySet()) {
            info.setProperty(e.getKey(), e.getValue());
        }

        info.save(new File(appDir, "Info.plist"));

        // Copy icons
        for (String icon : new String[] { "ios_icon_57.png", "ios_icon_114.png", "ios_icon_72.png", "ios_icon_144.png" }) {
            InputStream iconInput = getClass().getResourceAsStream(icon);
            File iconOutFile = new File(appDir, icon);
            FileUtils.copyInputStreamToFile(iconInput, iconOutFile);
            iconInput.close();
        }

        // Copy launch images
        HashMap<String, String> launchImageNames = new HashMap<String, String>();
        launchImageNames.put("ios_launch_320x480.png", "Default.png");
        launchImageNames.put("ios_launch_640x960.png", "Default@2x.png");
        launchImageNames.put("ios_launch_640x1136.png", "Default-568h@2x.png");
        launchImageNames.put("ios_launch_768x1024.png", "Default-Portrait-1024h.png");
        launchImageNames.put("ios_launch_1024x768.png", "Default-Landscape-1024h.png");
        launchImageNames.put("ios_launch_750x1334.png", "Default-Portrait-667h@2x.png");
        launchImageNames.put("ios_launch_1334x750.png", "Default-Landscape-667h@2x.png");
        launchImageNames.put("ios_launch_1242x2208.png", "Default-Portrait-736h@3x.png");
        launchImageNames.put("ios_launch_2208x1242.png", "Default-Landscape-736h@3x.png");
        launchImageNames.put("ios_launch_1536x2048.png", "Default-Portrait-1024h@2x.png");
        launchImageNames.put("ios_launch_2048x1536.png", "Default-Landscape-1024h@2x.png");
        launchImageNames.put("ios_launch_2048x2732.png", "Default-Portrait-1366h@2x.png");
        launchImageNames.put("ios_launch_2732x2048.png", "Default-Landscape-1366h@2x.png");
        launchImageNames.put("ios_launch_1125x2436.png", "Default-Portrait-812h@3x.png");
        launchImageNames.put("ios_launch_2436x1125.png", "Default-Landscape-812h@3x.png");
        for (HashMap.Entry<String, String> entry : launchImageNames.entrySet()) {
            InputStream launchImageInput = getClass().getResourceAsStream(entry.getKey());
            File laucnImageOutFile = new File(appDir, entry.getValue());
            FileUtils.copyInputStreamToFile(launchImageInput, laucnImageOutFile);
            launchImageInput.close();
        }

        // Copy Provisioning Profile
        FileUtils.copyFile(new File(provisioningProfile), new File(appDir, "embedded.mobileprovision"));

        // Copy bundle resources into .app folder
        ExtenderUtil.writeResourcesToDirectory(bundleResources, appDir);

        // Copy Executable
        FileUtils.copyFile(new File(exe), new File(appDir, FilenameUtils.getBaseName(exe)));

        // Extract entitlements
        File textProvisionFile = File.createTempFile("mobileprovision", ".plist");
        textProvisionFile.deleteOnExit();

        Exec.exec("security", "cms", "-D", "-i", provisioningProfile, "-o", textProvisionFile.getAbsolutePath());

        XMLPropertyListConfiguration decodedProvision = new XMLPropertyListConfiguration();
        decodedProvision.load(textProvisionFile);
        XMLPropertyListConfiguration entitlements = new XMLPropertyListConfiguration();
        entitlements.append(decodedProvision.configurationAt("Entitlements"));
        File entitlementOut = File.createTempFile("entitlement", ".xcent");
        entitlementOut.deleteOnExit();
        entitlements.save(entitlementOut);

        // Sign
        ProcessBuilder processBuilder = new ProcessBuilder("codesign", "-f", "-s", identity,
                                             "--entitlements", entitlementOut.getAbsolutePath(),
                                             appDir.getAbsolutePath());
        processBuilder.environment().put("EMBEDDED_PROFILE_NAME", "embedded.mobileprovision");
        processBuilder.environment().put("CODESIGN_ALLOCATE", TargetPlugin.getDefault().getCodeSignAllocatePath());

        Process process = processBuilder.start();

        try {
            InputStream errorIn = process.getErrorStream();
            ByteArrayOutputStream errorOut = new ByteArrayOutputStream();
            IOUtils.copy(errorIn, errorOut);
            errorIn.close();
            String errorMessage = new String(errorOut.toByteArray());

            int ret = process.waitFor();
            if (ret != 0) {
                logger.error(errorMessage);
                throw new IOException(errorMessage);
            }
        } catch (InterruptedException e1) {
            throw new RuntimeException(e1);
        }

        File zipFile = new File(packageDir, "Defold.ipa");
        ZipOutputStream zipStream = new ZipOutputStream(new FileOutputStream(zipFile));

        Collection<File> files = FileUtils.listFiles(appDir, null, true);
        String root = FilenameUtils.normalize(packageDir.getPath(), true);
        for (File f : files) {
            String p = FilenameUtils.normalize(f.getPath(), true);
            String rel = p.substring(root.length());

            // NOTE: The path is rooted. This works for over-the-air installation
            // but not via iTunes. See comment in IOSBundler for more information.
            // I don't dare to change this one to relative
            zipStream.putNextEntry(new ZipEntry("/Payload" + rel));

            FileInputStream input = new FileInputStream(f);
            IOUtils.copy(input, zipStream);
            input.close();
            zipStream.closeEntry();
        }

        zipStream.close();
        return zipFile.getAbsolutePath();
    }
}
