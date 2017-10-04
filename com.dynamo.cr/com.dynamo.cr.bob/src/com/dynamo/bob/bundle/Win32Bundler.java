package com.dynamo.bob.bundle;

import java.io.File;
import java.io.IOException;
import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.Collection;
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

public class Win32Bundler implements IBundler {
    @Override
    public void bundleApplication(Project project, File bundleDir)
            throws IOException, CompileExceptionError {
        bundleApplicationForPlatform(Platform.X86Win32, project, bundleDir);
    }

    public void bundleApplicationForPlatform(Platform platform, Project project, File bundleDir)
            throws IOException, CompileExceptionError {

        // Collect bundle/package resources to be included in bundle directory
        Map<String, IResource> bundleResources = ExtenderUtil.collectResources(project, platform);

        BobProjectProperties projectProperties = project.getProjectProperties();
        boolean debug = project.hasOption("debug");

        String extenderExeDir = FilenameUtils.concat(project.getRootDirectory(), "build");
        File extenderExe = new File(FilenameUtils.concat(extenderExeDir, FilenameUtils.concat(platform.getExtenderPair(), platform.formatBinaryName("dmengine"))));
        File defaultExe = new File(Bob.getDmengineExe(platform, debug));
        File bundleExe = defaultExe;
        if (extenderExe.exists()) {
            bundleExe = extenderExe;
        }

        String title = projectProperties.getStringValue("project", "title", "Unnamed");

        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());
        File appDir = new File(bundleDir, title);

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();

        // Copy archive and game.projectc
        for (String name : Arrays.asList("game.projectc", "game.arci", "game.arcd", "game.dmanifest", "game.public.der")) {
            FileUtils.copyFile(new File(buildDir, name), new File(appDir, name));
        }

        // Touch both OpenAL32.dll and wrap_oal.dll so they get included in the step below
        Bob.getLib(platform, "OpenAL32");
        Bob.getLib(platform, "wrap_oal");

        // Copy Executable and DLL:s
        String exeName = String.format("%s.exe", title);
        File exeOut = new File(appDir, exeName);
        FileUtils.copyFile(bundleExe, exeOut);
        Collection<File> dlls = FileUtils.listFiles(defaultExe.getParentFile(), new String[] {"dll"}, false);
        for (File file : dlls) {
            FileUtils.copyFileToDirectory(file, appDir);
        }

        // If windows.iap_provider is set to Gameroom we need to output a "launch" file that FB Gameroom understands.
        String iapProvider = projectProperties.getStringValue("windows", "iap_provider", "");
        if (iapProvider.equalsIgnoreCase("Gameroom"))
        {
            File launchFile = new File(appDir, "launch");
            String launchFileContent = String.format("%s $gameroom_args game.projectc", exeName);
            FileUtils.writeStringToFile(launchFile, launchFileContent, Charset.defaultCharset());
        }

        // Copy bundle resources into bundle directory
        ExtenderUtil.writeResourcesToDirectory(bundleResources, appDir);

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
