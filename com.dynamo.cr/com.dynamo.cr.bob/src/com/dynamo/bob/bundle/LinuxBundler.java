// Copyright 2020 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

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

        // Copy executable
        File bundleExe = bundleExes.get(0);
        File exeOut = new File(appDir, exeName + ".x86_64");
        FileUtils.copyFile(bundleExe, exeOut);
        exeOut.setExecutable(true);

        // Currently the stripping is left out from here
        // This is because the user can easily make a copy of the executable,
        // and then call "strip" to produce a slim executable
    }

    @Override
    public void bundleApplication(Project project, File bundleDir, ICanceled canceled)
            throws IOException, CompileExceptionError {

        final Platform platform = Platform.X86_64Linux;
        final List<Platform> architectures = Platform.getArchitecturesFromString(project.option("architectures", ""), platform);

        BobProjectProperties projectProperties = project.getProjectProperties();
        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        String exeName = BundleHelper.projectNameToBinaryName(title);
        File appDir = new File(bundleDir, title);

        BundleHelper.throwIfCanceled(canceled);

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();

        // In order to make a transition period, while phasing out 32 bit Darwin/Linux support completely, we will only support 64 bit for extensions
        final List<String> extensionFolders = ExtenderUtil.getExtensionFolders(project);
        final boolean hasExtensions = !extensionFolders.isEmpty();

        BundleHelper.throwIfCanceled(canceled);

        bundleApplicationForPlatform(platform, project, appDir, exeName);

        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());

        BundleHelper.throwIfCanceled(canceled);

        // Copy archive and game.projectc
        for (String name : Arrays.asList("game.projectc", "game.arci", "game.arcd", "game.dmanifest", "game.public.der")) {
            FileUtils.copyFile(new File(buildDir, name), new File(appDir, name));
        }

        BundleHelper.throwIfCanceled(canceled);

        // Collect bundle/package resources to be included in bundle directory
        Map<String, IResource> bundleResources = ExtenderUtil.collectBundleResources(project, architectures);

        BundleHelper.throwIfCanceled(canceled);

        // Copy bundle resources into bundle directory
        ExtenderUtil.writeResourcesToDirectory(bundleResources, appDir);
    }
}
