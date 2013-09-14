package com.dynamo.cr.target.bundle;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.configuration.plist.XMLPropertyListConfiguration;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.cr.editor.core.ProjectProperties;

public class OSXBundler {
    public static final String ICON_NAME = "icon.icns";
    private static List<PropertyAlias> propertyAliases = new ArrayList<OSXBundler.PropertyAlias>();

    static class PropertyAlias {
        String bundleProperty;
        String category;
        String key;
        String defaultValue;

        PropertyAlias(String bundleProperty, String category, String key, String defaultValue) {
            this.bundleProperty = bundleProperty;
            this.category = category;
            this.key = key;
            this.defaultValue = defaultValue;
        }
    }

    private static void addProperty(String bundleProperty, String category, String key, String defaultValue) {
        PropertyAlias alias = new PropertyAlias(bundleProperty, category, key, defaultValue);
        propertyAliases.add(alias);
    }

    static {
        addProperty("CFBundleIdentifier", "osx", "bundle_identifier", "Unnamed");
        addProperty("CFBundleShortVersionString", "project", "version", "1.0");
    }

    private String contentRoot;
    private ProjectProperties projectProperties;
    private File appDir;
    private String exe;
    private File contentsDir;
    private File resourcesDir;
    private File macosDir;
    private String projectRoot;

    private void copyIcon() throws IOException {
        String name = projectProperties.getStringValue("osx", "app_icon");
        if (name != null) {
            File inFile = new File(projectRoot, name);
            File outFile = new File(resourcesDir, ICON_NAME);
            FileUtils.copyFile(inFile, outFile);
        }
    }

    /**
     *
     * @param projectProperties corresponding game.project file
     * @param exe path to executable
     * @param projectRoot project root
     * @param contentRoot path to *compiled* content
     * @param outputDir output directory
     */
    public OSXBundler(ProjectProperties projectProperties, String exe, String projectRoot, String contentRoot, String outputDir) {
        this.projectProperties = projectProperties;
        this.exe = exe;
        this.projectRoot = projectRoot;
        this.contentRoot = contentRoot;

        File packageDir = new File(outputDir);
        appDir = new File(packageDir, projectProperties.getStringValue("project", "title", "Unnamed") + ".app");
        contentsDir = new File(appDir, "Contents");
        resourcesDir = new File(contentsDir, "Resources");
        macosDir = new File(contentsDir, "MacOS");
    }

    public void bundleApplication() throws IOException, ConfigurationException {
        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();
        contentsDir.mkdirs();
        resourcesDir.mkdirs();
        macosDir.mkdirs();

        // Copy archive and game.projectc
        for (String name : Arrays.asList("game.projectc", "game.darc")) {
            FileUtils.copyFile(new File(contentRoot, name), new File(resourcesDir, name));
        }

        // Create Info.plist
        InputStream infoIn = getClass().getResourceAsStream("resources/osx/Info.plist");
        XMLPropertyListConfiguration info = new XMLPropertyListConfiguration();
        info.load(infoIn);
        infoIn.close();

        // Set properties from project file
        for (PropertyAlias alias : propertyAliases) {
            info.setProperty(alias.bundleProperty, projectProperties.getStringValue(alias.category, alias.key, alias.defaultValue));
        }
        info.setProperty("CFBundleExecutable", FilenameUtils.getName(exe));

        // Copy icon
        copyIcon();

        // Save updated Info.plist
        info.save(new File(contentsDir, "Info.plist"));

        // Copy Executable
        File exeOut = new File(macosDir, FilenameUtils.getName(exe));
        FileUtils.copyFile(new File(exe), exeOut);
        exeOut.setExecutable(true);
    }
}
