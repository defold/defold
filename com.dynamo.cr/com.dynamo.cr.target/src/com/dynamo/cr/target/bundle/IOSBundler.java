package com.dynamo.cr.target.bundle;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.configuration.SubnodeConfiguration;
import org.apache.commons.configuration.plist.XMLPropertyListConfiguration;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class IOSBundler {
    private static Logger logger = LoggerFactory.getLogger(IOSBundler.class);

    public void bundleApplication(String identity, String provisioningProfile, String exe, String contentRoot, String outputDir, Collection<String> icons, boolean preRenderedIcons, Map<String, String> properties) throws IOException, ConfigurationException {
        File packageDir = new File(outputDir);
        File appDir = new File(packageDir, properties.get("CFBundleDisplayName") + ".app");
        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();

        // Copy content
        FileUtils.copyDirectory(new File(contentRoot), appDir);

        // Create Info.plist
        InputStream infoIn = getClass().getResourceAsStream("resources/ios/Info.plist");
        XMLPropertyListConfiguration info = new XMLPropertyListConfiguration();
        info.load(infoIn);
        infoIn.close();
        for (Entry<String, String> e : properties.entrySet()) {
            info.setProperty(e.getKey(), e.getValue());
        }

        // Copy ResourceRules.plist
        InputStream resourceRulesIn = getClass().getResourceAsStream("resources/ios/ResourceRules.plist");
        File resourceRulesOutFile = new File(appDir, "ResourceRules.plist");
        FileUtils.copyInputStreamToFile(resourceRulesIn, resourceRulesOutFile);
        resourceRulesIn.close();

        // Copy icons
        List<String> iconNames = new ArrayList<String>();
        for (String icon : icons) {
            File iconInFile = new File(icon);
            File iconOutFile = new File(appDir, iconInFile.getName());
            iconNames.add(iconInFile.getName());
            FileUtils.copyFile(new File(icon), iconOutFile);
        }
        SubnodeConfiguration primaryIcon = info.configurationAt("CFBundleIcons", true).configurationAt("CFBundlePrimaryIcon", true);
        primaryIcon.setProperty("CFBundleIconFiles", iconNames);
        primaryIcon.setProperty("UIPrerenderedIcon", preRenderedIcons);

        // Save updated Info.plist
        info.save(new File(appDir, "Info.plist"));

        // Copy Provisioning Profile
        FileUtils.copyFile(new File(provisioningProfile), new File(appDir, "embedded.mobileprovision"));

        // Copy Executable
        FileUtils.copyFile(new File(exe), new File(appDir, FilenameUtils.getBaseName(exe)));

        // Sign
        ProcessBuilder processBuilder = new ProcessBuilder("codesign", "-f", "-s", identity,
                                             "--resource-rules=" + resourceRulesOutFile.getAbsolutePath(),
                                             appDir.getAbsolutePath());
        processBuilder.environment().put("EMBEDDED_PROFILE_NAME", "embedded.mobileprovision");

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
    }
}
