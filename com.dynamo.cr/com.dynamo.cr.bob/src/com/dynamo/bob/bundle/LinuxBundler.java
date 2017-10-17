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

public class LinuxBundler implements IBundler {

    public void bundleApplicationForPlatform(Platform platform, Project project, File appDir, String title)
            throws IOException, CompileExceptionError {
        String extenderExeDir = FilenameUtils.concat(project.getRootDirectory(), "build");
        File extenderExe = new File(FilenameUtils.concat(extenderExeDir, FilenameUtils.concat(platform.getExtenderPair(), platform.formatBinaryName("dmengine"))));
        File defaultExe = new File(Bob.getDmengineExe(platform, project.hasOption("debug")));
        File bundleExe = defaultExe;
        if (extenderExe.exists()) {
            bundleExe = extenderExe;
        }

        File exeOut;
        if (platform == Platform.X86Linux)
            exeOut = new File(appDir, title + ".x86");
        else
            exeOut = new File(appDir, title + ".x86_64");

        FileUtils.copyFile(bundleExe, exeOut);
        exeOut.setExecutable(true);
    }

    @Override
    public void bundleApplication(Project project, File bundleDir)
            throws IOException, CompileExceptionError {

        BobProjectProperties projectProperties = project.getProjectProperties();
        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        File appDir = new File(bundleDir, title);

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();

        // In order to make a transition period, while phasing out 32 bit Darwin/Linux support completely, we will only support 64 bit for extensions
        final List<String> extensionFolders = ExtenderUtil.getExtensionFolders(project);
        final boolean hasExtensions = !extensionFolders.isEmpty();
        
        Platform primaryPlatform = Platform.X86_64Linux;
        bundleApplicationForPlatform(primaryPlatform, project, appDir, title);

        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());

        // Copy archive and game.projectc
        for (String name : Arrays.asList("game.projectc", "game.arci", "game.arcd", "game.dmanifest", "game.public.der")) {
            FileUtils.copyFile(new File(buildDir, name), new File(appDir, name));
        }
        
        // Collect bundle/package resources to be included in bundle directory
        Map<String, IResource> bundleResources = ExtenderUtil.collectResources(project, primaryPlatform);

        // Copy bundle resources into bundle directory
        ExtenderUtil.writeResourcesToDirectory(bundleResources, appDir);
    }
}
