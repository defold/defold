package com.dynamo.bob.bundle;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;

import org.apache.commons.io.FileUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.util.BobProjectProperties;

public class LinuxBundler implements IBundler {

    @Override
    public void bundleApplication(Project project, File bundleDir)
            throws IOException, CompileExceptionError {

        BobProjectProperties projectProperties = project.getProjectProperties();
        String exe = Bob.getDmengineExe(Platform.X86Linux, project.hasOption("debug"));
        String title = projectProperties.getStringValue("project", "title", "Unnamed");

        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());
        File appDir = new File(bundleDir, title);

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();

        // Copy archive and game.projectc
        for (String name : Arrays.asList("game.projectc", "game.darc")) {
            FileUtils.copyFile(new File(buildDir, name), new File(appDir, name));
        }

        // Copy Executable
        File exeOut = new File(appDir, title);
        FileUtils.copyFile(new File(exe), exeOut);
        exeOut.setExecutable(true);
    }
}
