package com.dynamo.cr.target.bundle;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.io.FileUtils;

import com.dynamo.cr.editor.core.ProjectProperties;

public class LinuxBundler {
    private String contentRoot;
    private File appDir;
    private String exe;
    private String title;

    /**
     *
     * @param projectProperties corresponding game.project file
     * @param exe path to executable
     * @param projectRoot project root
     * @param contentRoot path to *compiled* content
     * @param outputDir output directory
     */
    public LinuxBundler(ProjectProperties projectProperties, String exe, String projectRoot, String contentRoot, String outputDir) {
        this.exe = exe;
        this.contentRoot = contentRoot;

        File packageDir = new File(outputDir);
        this.title = projectProperties.getStringValue("project", "title", "Unnamed");
        appDir = new File(packageDir, title);
    }

    public void bundleApplication() throws IOException, ConfigurationException {
        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();

        // Copy archive and game.projectc
        for (String name : Arrays.asList("game.projectc", "game.darc")) {
            FileUtils.copyFile(new File(contentRoot, name), new File(appDir, name));
        }

        // Copy Executable
        File exeOut = new File(appDir, title);
        FileUtils.copyFile(new File(exe), exeOut);
        exeOut.setExecutable(true);
    }
}
