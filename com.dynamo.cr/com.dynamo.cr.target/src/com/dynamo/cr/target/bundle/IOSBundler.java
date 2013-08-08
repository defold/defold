package com.dynamo.cr.target.bundle;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.configuration.plist.XMLPropertyListConfiguration;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.common.util.Exec;
import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.target.core.TargetPlugin;
import com.samskivert.mustache.Mustache;
import com.samskivert.mustache.Template;

public class IOSBundler {
    private static Logger logger = LoggerFactory.getLogger(IOSBundler.class);
    private ProjectProperties projectProperties;
    private String exe;
    private String projectRoot;
    private String contentRoot;
    private String title;
    private File appDir;
    private String identity;
    private String provisioningProfile;
    private File packageDir;

    private static List<PropertyAlias> propertyAliases = new ArrayList<IOSBundler.PropertyAlias>();

    static class PropertyAlias {
        String bundleProperty;
        String category;
        String key;
        String defaultValue;

        PropertyAlias(String bundleProperty, String category, String key,
                String defaultValue) {
            this.bundleProperty = bundleProperty;
            this.category = category;
            this.key = key;
            this.defaultValue = defaultValue;
        }
    }

    private static void addProperty(String bundleProperty, String category,
            String key, String defaultValue) {
        PropertyAlias alias = new PropertyAlias(bundleProperty, category, key,
                defaultValue);
        propertyAliases.add(alias);
    }

    static {
        addProperty("CFBundleIdentifier", "ios", "bundle_identifier", "example.unnamed");
        addProperty("CFBundleVersion", "project", "version", "1.0");
        addProperty("CFBundleShortVersionString", "project", "version", "1.0");
    }

    private void copyIcon(String name, String outName)
            throws IOException {
        String resource = projectProperties.getStringValue("ios", name);
        if (resource != null && resource.length() > 0) {
            File inFile = new File(projectRoot, resource);
            File outFile = new File(appDir, outName);
            FileUtils.copyFile(inFile, outFile);
        }
    }

    /**
     *
     * @param projectProperties
     *            corresponding game.project file
     * @param exe
     *            path to executable
     * @param projectRoot
     *            project root
     * @param contentRoot
     *            path to *compiled* content
     * @param outputDir
     *            output directory
     */
    public IOSBundler(String identity, String provisioningProfile,
            ProjectProperties projectProperties, String exe,
            String projectRoot, String contentRoot, String outputDir) {
        this.identity = identity;
        this.provisioningProfile = provisioningProfile;
        this.projectProperties = projectProperties;
        this.exe = exe;
        this.projectRoot = projectRoot;
        this.contentRoot = contentRoot;

        packageDir = new File(outputDir);
        this.title = projectProperties.getStringValue("project", "title",
                "Unnamed");
        appDir = new File(packageDir, title + ".app");
    }

    public void bundleApplication() throws IOException, ConfigurationException {
        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();

        /*
         * NOTE: Archive is currently disabled on iOS due to compression problems
         * It seems that game.arc isn't compressed in the final ipa. File size limitation?
         */
        boolean useArchive = false;

        if (useArchive) {
            // Copy archive and game.projectc
            for (String name : Arrays.asList("game.projectc", "game.arc")) {
                FileUtils.copyFile(new File(contentRoot, name), new File(appDir, name));
            }
        } else {
            FileUtils.copyDirectory(new File(contentRoot), appDir);
            new File(appDir, "game.arc").delete();
        }

        // Create Info.plist
        InputStream infoIn = getClass().getResourceAsStream(
                "resources/ios/Info.plist");

        Template infoTemplate = Mustache.compiler().compile(IOUtils.toString(infoIn));
        infoIn.close();
        Map<String, Object> infoData = new HashMap<String, Object>();

        // Set properties from project file
        for (PropertyAlias alias : propertyAliases) {
            String value = projectProperties.getStringValue(alias.category, alias.key, alias.defaultValue);
            infoData.put(alias.bundleProperty, value);
        }
        infoData.put("CFBundleDisplayName", title);
        infoData.put("CFBundleExecutable", FilenameUtils.getName(exe));

        // Copy ResourceRules.plist
        InputStream resourceRulesIn = getClass().getResourceAsStream(
                "resources/ios/ResourceRules.plist");
        File resourceRulesOutFile = new File(appDir, "ResourceRules.plist");
        FileUtils.copyInputStreamToFile(resourceRulesIn, resourceRulesOutFile);
        resourceRulesIn.close();

        // Copy icons
        copyIcon("app_icon_57x57", "ios_icon_57.png");
        copyIcon("app_icon_114x114", "ios_icon_114.png");
        copyIcon("app_icon_72x72", "ios_icon_72.png");
        copyIcon("app_icon_144x144", "ios_icon_144.png");

        // Copy launch images
        copyIcon("launch_image_320x480", "Default.png");
        copyIcon("launch_image_640x960", "Default@2x.png");
        copyIcon("launch_image_640x1136", "Default-568h@2x.png");
        copyIcon("launch_image_768x1004", "Default-Portrait~ipad.png");
        copyIcon("launch_image_1536x2008", "Default-Portrait@2x~ipad.png");
        copyIcon("launch_image_1024x748", "Default-Landscape~ipad.png");
        copyIcon("launch_image_2048x1496", "Default-Landscape@2x~ipad.png");

        // NOTE: We don't set CFBundleIconFiles here
        // Instead we copy icons to pre-set names, ios_icon_X.png
        // Legacy solution due to a bug in XMLPropertyListConfiguration previously used
        infoData.put("UIPrerenderedIcon", projectProperties.getBooleanValue("ios", "pre_renderered_icons", false));

        String facebookAppId = projectProperties.getStringValue("facebook", "appid", null);
        List<String> urlSchemes = new ArrayList<String>();

        if (facebookAppId != null) {
            urlSchemes.add(facebookAppId);
        }
        infoData.put("CFBundleURLSchemes", urlSchemes);

        // Save updated Info.plist
        File infoFile = new File(appDir, "Info.plist");
        String compiledInfo = infoTemplate.execute(infoData);
        FileUtils.write(infoFile, compiledInfo);

        // Copy Provisioning Profile
        FileUtils.copyFile(new File(provisioningProfile), new File(appDir,
                "embedded.mobileprovision"));

        // Copy Executable
        FileUtils.copyFile(new File(exe),
                new File(appDir, FilenameUtils.getName(exe)));

        // Sign
        if (identity != null && provisioningProfile != null) {
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

            ProcessBuilder processBuilder = new ProcessBuilder("codesign",
                    "-f", "-s", identity, "--resource-rules="
                            + resourceRulesOutFile.getAbsolutePath(),
                    "--entitlements", entitlementOut.getAbsolutePath(),
                    appDir.getAbsolutePath());
            processBuilder.environment().put("EMBEDDED_PROFILE_NAME",
                    "embedded.mobileprovision");
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
        }

        File zipFile = new File(this.packageDir, this.title + ".ipa");
        ZipOutputStream zipStream = new ZipOutputStream(new FileOutputStream(zipFile));

        Collection<File> files = FileUtils.listFiles(appDir, null, true);
        String root = FilenameUtils.normalize(packageDir.getPath(), true);
        for (File f : files) {
            String p = FilenameUtils.normalize(f.getPath(), true);
            String rel = p.substring(root.length());

            // NOTE: The path to Payload is relative, i.e. not /Payload
            // If rooted iTunes complains about invalid package
            zipStream.putNextEntry(new ZipEntry("Payload" + rel));

            FileInputStream input = new FileInputStream(f);
            IOUtils.copy(input, zipStream);
            input.close();
            zipStream.closeEntry();
        }

        zipStream.close();
    }

}
