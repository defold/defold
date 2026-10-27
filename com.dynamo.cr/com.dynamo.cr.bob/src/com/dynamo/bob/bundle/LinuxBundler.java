// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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
import java.util.List;
import java.util.Map;

import org.apache.commons.io.FileUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.EngineArtifactsProvider;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.util.BobProjectProperties;
import org.apache.commons.io.FilenameUtils;

@BundlerParams(platforms = {"x86_64-linux", "arm64-linux"})
public class LinuxBundler implements IBundler {
    private static Logger logger = Logger.getLogger(LinuxBundler.class.getName());

    @Override
    public IResource getManifestResource(Project project, Platform platform) throws IOException {
        return null;
    }

    @Override
    public String getMainManifestName(Platform platform) {
        return null;
    }

    @Override
    public String getMainManifestTargetPath(Platform platform) {
        return null;
    }

    @Override
    public void updateManifestProperties(Project project, Platform platform,
                                BobProjectProperties projectProperties,
                                Map<String, Map<String, Object>> propertiesMap,
                                Map<String, Object> properties) throws IOException {
    }

    public void bundleApplicationForPlatform(Platform platform, Project project, File appDir, String exeName)
            throws IOException, CompileExceptionError {
        List<File> bundleExes = ExtenderUtil.getNativeExtensionEngineBinaries(project, platform);
        final String variant = project.option("variant", Bob.VARIANT_RELEASE);
        if (bundleExes == null) {
            bundleExes = Bob.getDefaultDmengineFiles(platform, variant);
        } else {
            if (variant.equals("debug")) {
                File debugEngine = new File(bundleExes.get(0).getParent(), EngineArtifactsProvider.LINUX_UNSTRIPPED_ENGINE_NAME);
                bundleExes.set(0, debugEngine);
            }
        }
        if (bundleExes.size() > 1) {
            throw new IOException("Invalid number of binaries for Linux when bundling: " + bundleExes.size());
        }

        // Copy executable
        File bundleExe = bundleExes.get(0);
        File exeOut = new File(appDir, getExeFilename(platform, exeName));
        FileUtils.copyFile(bundleExe, exeOut);
        exeOut.setExecutable(true);

        // Currently the stripping is left out from here
        // This is because the user can easily make a copy of the executable,
        // and then call "strip" to produce a slim executable
    }

    static String getExeFilename(Platform platform, String exeName) {
        return exeName + (platform.equals(Platform.X86_64Linux) ? ".x86_64" : ".arm64");
    }

    // The unstripped engine is placed next to the app folder, so it isn't shipped with the game
    public static void copySymbols(Project project, Platform platform, File binaryDir, File bundleDir, String exeName) throws IOException {
        if (!project.hasOption("with-symbols")) {
            return;
        }
        File unstripped = new File(binaryDir, EngineArtifactsProvider.LINUX_UNSTRIPPED_ENGINE_NAME);
        if (!unstripped.exists()) {
            logger.warning("Debug symbols not found: %s", unstripped.getAbsolutePath());
            return;
        }
        File symbolsOut = new File(bundleDir, getExeFilename(platform, exeName) + ".debug");
        logger.info("Copy debug symbols " + symbolsOut);
        FileUtils.copyFile(unstripped, symbolsOut);
    }

    @Override
    public void bundleApplication(Project project, Platform platform, File bundleDir, ICanceled canceled)
            throws IOException, CompileExceptionError {

        final List<Platform> architectures = Platform.getArchitecturesFromString(project.option("architectures", ""), platform);

        BobProjectProperties projectProperties = project.getProjectProperties();
        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        String exeName = BundleHelper.projectNameToBinaryName(title);
        File appDir = new File(bundleDir, title);

        BundleHelper.throwIfCanceled(canceled);

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();

        BundleHelper.throwIfCanceled(canceled);

        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());
        bundleApplicationForPlatform(platform, project, appDir, exeName);

        File binaryDir = new File(FilenameUtils.concat(project.getBinaryOutputDirectory(), platform.getExtenderPair()));
        BundleHelper.copySharedLibraries(platform, binaryDir, appDir);
        copySymbols(project, platform, binaryDir, bundleDir, exeName);

        BundleHelper.throwIfCanceled(canceled);

        if (BundleHelper.isArchiveIncluded(project)) {
            // Copy archive and game.projectc
            for (String name : BundleHelper.getArchiveFilenames(buildDir)) {
                FileUtils.copyFile(new File(buildDir, name), new File(appDir, name));
            }
        }

        BundleHelper.throwIfCanceled(canceled);

        // Collect bundle/package resources to be included in bundle directory
        Map<String, IResource> bundleResources = ExtenderUtil.collectBundleResources(project, architectures);

        BundleHelper.throwIfCanceled(canceled);

        // Copy bundle resources into bundle directory
        ExtenderUtil.writeResourcesToDirectory(bundleResources, appDir);

        BundleHelper.moveBundleIfNeed(project, bundleDir);
    }
}
