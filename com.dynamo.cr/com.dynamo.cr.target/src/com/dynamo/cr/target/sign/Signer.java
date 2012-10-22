package com.dynamo.cr.target.sign;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Collection;
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

import com.dynamo.cr.target.core.TargetPlugin;
import com.google.common.io.Files;

public class Signer {
    private static Logger logger = LoggerFactory.getLogger(Signer.class);

    public String sign(String identity, String provisioningProfile, String exe, Map<String, String> properties) throws IOException, ConfigurationException {
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

        // Copy ResourceRules.plist
        InputStream resourceRulesIn = getClass().getResourceAsStream("ResourceRules.plist");
        File resourceRulesOutFile = new File(appDir, "ResourceRules.plist");
        FileUtils.copyInputStreamToFile(resourceRulesIn, resourceRulesOutFile);
        resourceRulesIn.close();

        // Copy icons
        for (String icon : new String[] { "ios_icon_57.png", "ios_icon_114.png", "ios_icon_72.png", "ios_icon_144.png" }) {
            InputStream iconInput = getClass().getResourceAsStream(icon);
            File iconOutFile = new File(appDir, icon);
            FileUtils.copyInputStreamToFile(iconInput, iconOutFile);
            iconInput.close();
        }

        // Copy Provisioning Profile
        FileUtils.copyFile(new File(provisioningProfile), new File(appDir, "embedded.mobileprovision"));

        // Copy Executable
        FileUtils.copyFile(new File(exe), new File(appDir, FilenameUtils.getBaseName(exe)));

        // Sign
        ProcessBuilder processBuilder = new ProcessBuilder("codesign", "-f", "-s", identity,
                                             "--resource-rules=" + resourceRulesOutFile.getAbsolutePath(),
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
