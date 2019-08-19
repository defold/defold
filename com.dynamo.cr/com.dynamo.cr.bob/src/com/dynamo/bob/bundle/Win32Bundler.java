package com.dynamo.bob.bundle;

import java.io.File;
import java.io.IOException;
import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.Collection;
import java.util.Map;
import java.util.List;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.util.BobProjectProperties;

public class Win32Bundler implements IBundler {
    @Override
    public void bundleApplication(Project project, File bundleDir, ICanceled canceled)
            throws IOException, CompileExceptionError {
        bundleApplicationForPlatform(Platform.X86Win32, project, bundleDir, canceled);
    }

    public void bundleApplicationForPlatform(Platform platform, Project project, File bundleDir, ICanceled canceled)
            throws IOException, CompileExceptionError {

        BundleHelper.throwIfCanceled(canceled);

        // Collect bundle/package resources to be included in bundle directory
        Map<String, IResource> bundleResources = ExtenderUtil.collectBundleResources(project, platform);

        BundleHelper.throwIfCanceled(canceled);

        BobProjectProperties projectProperties = project.getProjectProperties();

        String extenderExeDir = FilenameUtils.concat(project.getRootDirectory(), "build");
        List<File> bundleExes = Bob.getNativeExtensionEngineBinaries(platform, extenderExeDir);
        if (bundleExes == null) {
            final String variant = project.option("variant", Bob.VARIANT_RELEASE);
            bundleExes = Bob.getDefaultDmengineFiles(platform, variant);
        }
        if (bundleExes.size() > 1) {
            throw new IOException("Invalid number of binaries for Windows when bundling: " + bundleExes.size());
        }
        File bundleExe = bundleExes.get(0);

        BundleHelper.throwIfCanceled(canceled);

        String title = projectProperties.getStringValue("project", "title", "Unnamed");

        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());
        File appDir = new File(bundleDir, title);

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();

        BundleHelper.throwIfCanceled(canceled);

        // Copy archive and game.projectc
        for (String name : Arrays.asList("game.projectc", "game.arci", "game.arcd", "game.dmanifest", "game.public.der")) {
            FileUtils.copyFile(new File(buildDir, name), new File(appDir, name));
        }

        BundleHelper.throwIfCanceled(canceled);

        // Touch both OpenAL32.dll and wrap_oal.dll so they get included in the step below
        String openal_dll = Bob.getLib(platform, "OpenAL32");
        String wrap_oal_dll = Bob.getLib(platform, "wrap_oal");

        // Copy Executable and DLL:s
        String exeName = String.format("%s.exe", BundleHelper.projectNameToBinaryName(title));
        File exeOut = new File(appDir, exeName);
        FileUtils.copyFile(bundleExe, exeOut);
        FileUtils.copyFileToDirectory(new File(openal_dll), appDir);
        FileUtils.copyFileToDirectory(new File(wrap_oal_dll), appDir);

        BundleHelper.throwIfCanceled(canceled);

        // Copy bundle resources into bundle directory
        ExtenderUtil.writeResourcesToDirectory(bundleResources, appDir);

        BundleHelper.throwIfCanceled(canceled);

        String icon = projectProperties.getStringValue("windows", "app_icon");
        if (icon != null) {
            File iconFile = new File(project.getRootDirectory(), icon);
            if (iconFile.exists()) {
                String[] args = new String[] { exeOut.getAbsolutePath(), iconFile.getAbsolutePath() };
                try {
                    IconExe.main(args);
                } catch (Exception e) {
                    throw new IOException("Failed to set icon for executable", e);
                }
            } else {
                throw new IOException("The icon does not exist: " + iconFile.getAbsolutePath());
            }
        }
    }
}
