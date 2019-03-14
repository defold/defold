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

    public void bundleApplicationForPlatform(Platform platform, Project project, File appDir, String exeName)
            throws IOException, CompileExceptionError {
        String extenderExeDir = FilenameUtils.concat(project.getRootDirectory(), "build");
        List<File> bundleExes = Bob.getNativeExtensionEngineBinaries(platform, extenderExeDir);
        if (bundleExes == null) {
            final String variant = project.option("variant", Bob.VARIANT_RELEASE);
            bundleExes = Bob.getDefaultDmengineFiles(platform, variant);
        }
        if (bundleExes.size() > 1) {
            throw new IOException("Invalid number of binaries for Linux when bundling: " + bundleExes.size());
        }
        File bundleExe = bundleExes.get(0);

        File exeOut = new File(appDir, exeName + ".x86_64");

        FileUtils.copyFile(bundleExe, exeOut);
        exeOut.setExecutable(true);
    }

    @Override
    public void bundleApplication(Project project, File bundleDir, ICanceled canceled)
            throws IOException, CompileExceptionError {

        final Platform platform = Platform.X86_64Linux;
        BobProjectProperties projectProperties = project.getProjectProperties();
        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        String exeName = BundleHelper.projectNameToBinaryName(title);
        File appDir = new File(bundleDir, title);

        if(canceled.isCanceled()) {
            return;
        }

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();

        // In order to make a transition period, while phasing out 32 bit Darwin/Linux support completely, we will only support 64 bit for extensions
        final List<String> extensionFolders = ExtenderUtil.getExtensionFolders(project);
        final boolean hasExtensions = !extensionFolders.isEmpty();

        if(canceled.isCanceled()) {
            return;
        }

        bundleApplicationForPlatform(platform, project, appDir, exeName);

        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());

        if(canceled.isCanceled()) {
            return;
        }

        // Copy archive and game.projectc
        for (String name : Arrays.asList("game.projectc", "game.arci", "game.arcd", "game.dmanifest", "game.public.der")) {
            FileUtils.copyFile(new File(buildDir, name), new File(appDir, name));
        }

        if(canceled.isCanceled()) {
            return;
        }

        // Collect bundle/package resources to be included in bundle directory
        Map<String, IResource> bundleResources = ExtenderUtil.collectResources(project, platform);

        if(canceled.isCanceled()) {
            return;
        }

        // Copy bundle resources into bundle directory
        ExtenderUtil.writeResourcesToDirectory(bundleResources, appDir);
    }
}
