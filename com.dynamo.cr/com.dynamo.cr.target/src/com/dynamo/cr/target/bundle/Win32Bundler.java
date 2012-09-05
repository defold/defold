package com.dynamo.cr.target.bundle;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.cr.editor.core.ProjectProperties;

public class Win32Bundler {
    private ProjectProperties projectProperties;
    private String projectRoot;
    private String contentRoot;
    private File appDir;
    private String exe;

    /**
     *
     * @param projectProperties corresponding game.project file
     * @param exe path to executable
     * @param projectRoot project root
     * @param contentRoot path to *compiled* content
     * @param outputDir output directory
     */
    public Win32Bundler(ProjectProperties projectProperties, String exe, String projectRoot, String contentRoot, String outputDir) {
        this.projectProperties = projectProperties;
        this.exe = exe;
        this.projectRoot = projectRoot;
        this.contentRoot = contentRoot;

        File packageDir = new File(outputDir);
        appDir = new File(packageDir, projectProperties.getStringValue("project", "title", "Unnamed"));
    }

    public void bundleApplication() throws IOException, ConfigurationException {
        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();

        // Copy archive and game.projectc
        for (String name : Arrays.asList("game.projectc", "game.arc")) {
            FileUtils.copyFile(new File(contentRoot, name), new File(appDir, name));
        }

        // Copy Executable
        File exeOut = new File(appDir, FilenameUtils.getName(exe));
        FileUtils.copyFile(new File(exe), exeOut);

        String icon = projectProperties.getStringValue("windows", "app_icon");
        if (icon != null) {
            File iconFile = new File(projectRoot, icon);
            if (iconFile.exists()) {
                String[] args = new String[] { exe, iconFile.getAbsolutePath() };
                try {
                    IconExe.main(args);
                } catch (Exception e) {
                    throw new IOException("Failed to set icon for executable", e);
                }
            }
        }
    }
}
