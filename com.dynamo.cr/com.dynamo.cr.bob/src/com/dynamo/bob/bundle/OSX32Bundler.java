package com.dynamo.bob.bundle;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.util.BobProjectProperties;

public class OSX32Bundler implements IBundler {
    public static final String ICON_NAME = "icon.icns";

    private void copyIcon(BobProjectProperties projectProperties, File projectRoot, File resourcesDir) throws IOException {
        String name = projectProperties.getStringValue("osx", "app_icon");
        if (name != null) {
            File inFile = new File(projectRoot, name);
            File outFile = new File(resourcesDir, ICON_NAME);
            FileUtils.copyFile(inFile, outFile);
        }
    }

    @Override
    public void bundleApplication(Project project, File bundleDir)
            throws IOException, CompileExceptionError {
        bundleApplicationForPlatform(Platform.X86Darwin, project, bundleDir);
    }

    public void bundleApplicationForPlatform(Platform platform, Project project, File bundleDir)
            throws IOException, CompileExceptionError {

        BobProjectProperties projectProperties = project.getProjectProperties();
        String title = projectProperties.getStringValue("project", "title", "Unnamed");

        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());
        File appDir = new File(bundleDir, title + ".app");
        File contentsDir = new File(appDir, "Contents");
        File resourcesDir = new File(contentsDir, "Resources");
        File macosDir = new File(contentsDir, "MacOS");

        boolean debug = project.hasOption("debug");

        String extenderExeDir = FilenameUtils.concat(project.getRootDirectory(), "build");
        File extenderExe = new File(FilenameUtils.concat(extenderExeDir, FilenameUtils.concat(platform.getExtenderPair(), platform.formatBinaryName("dmengine"))));
        File defaultExe = new File(Bob.getDmengineExe(platform, debug));
        File bundleExe = defaultExe;
        if (extenderExe.exists()) {
            bundleExe = extenderExe;
        }

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();
        contentsDir.mkdirs();
        resourcesDir.mkdirs();
        macosDir.mkdirs();

        BundleHelper helper = new BundleHelper(project, platform, bundleDir, ".app");

        // Collect bundle/package resources to be included in .App directory
        Map<String, IResource> bundleResources = ExtenderUtil.collectResources(project, platform);

        // Copy bundle resources into .app folder
        ExtenderUtil.writeResourcesToDirectory(bundleResources, appDir);

        // Copy archive and game.projectc
        for (String name : Arrays.asList("game.projectc", "game.arci", "game.arcd", "game.dmanifest", "game.public.der")) {
            FileUtils.copyFile(new File(buildDir, name), new File(resourcesDir, name));
        }

        helper.format("osx", "infoplist", new File(contentsDir, "Info.plist"));

        // Copy icon
        copyIcon(projectProperties, new File(project.getRootDirectory()), resourcesDir);

        // Copy Executable
        File exeOut = new File(macosDir, title);
        FileUtils.copyFile(bundleExe, exeOut);
        exeOut.setExecutable(true);
    }

}
