package com.dynamo.cr.target.bundle;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.Collection;

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
    private String title;

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
        this.title = projectProperties.getStringValue("project", "title", "Unnamed");
        appDir = new File(packageDir, title);
    }

    public void bundleApplication() throws IOException, ConfigurationException {
        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();

        // Copy archive and game.projectc
        for (String name : Arrays.asList("game.projectc", "game.arc")) {
            FileUtils.copyFile(new File(contentRoot, name), new File(appDir, name));
        }

        // Copy Executable and DLL:s
        File exeOut = new File(appDir, String.format("%s.exe", title));
        FileUtils.copyFile(new File(exe), exeOut);
        Collection<File> dlls = FileUtils.listFiles(new File(FilenameUtils.getFullPath(exe)), new String[] {"dll"}, false);
        for (File file : dlls) {
            FileUtils.copyFileToDirectory(file, appDir);
        }

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
