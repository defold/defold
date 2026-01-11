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

@BundlerParams(platforms = {"x86_64-win32", "x86-win32"})
public class Win32Bundler implements IBundler {

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

    @Override
    public void bundleApplication(Project project, Platform platform, File bundleDir, ICanceled canceled)
            throws IOException, CompileExceptionError {

        BundleHelper.throwIfCanceled(canceled);

        final List<Platform> architectures = Platform.getArchitecturesFromString(project.option("architectures", ""), platform);

        // Collect bundle/package resources to be included in bundle directory
        Map<String, IResource> bundleResources = ExtenderUtil.collectBundleResources(project, architectures);

        BundleHelper.throwIfCanceled(canceled);

        BobProjectProperties projectProperties = project.getProjectProperties();

        List<File> bundleExes = ExtenderUtil.getNativeExtensionEngineBinaries(project, platform);
        if (bundleExes == null) {
            final String variant = project.option("variant", Bob.VARIANT_RELEASE);
            bundleExes = Bob.getDefaultDmengineFiles(platform, variant);
        }
        if (bundleExes.size() > 1) {
            throw new IOException("Invalid number of binaries for Windows when bundling: " + bundleExes.size());
        }
        File bundleExe = bundleExes.get(0);

        BundleHelper.throwIfCanceled(canceled);

        String title = projectProperties.getStringValue("project", "title", "htmlUnnamed");

        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());
        File appDir = new File(bundleDir, title);

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();

        BundleHelper.throwIfCanceled(canceled);

        if (BundleHelper.isArchiveIncluded(project)) {
            // Copy archive and game.projectc
            for (String name : BundleHelper.getArchiveFilenames(buildDir)) {
                FileUtils.copyFile(new File(buildDir, name), new File(appDir, name));
            }
        }

        BundleHelper.throwIfCanceled(canceled);

        // Copy Executable and DLL:s
        String exeName = String.format("%s.exe", BundleHelper.projectNameToBinaryName(title));
        File exeOut = new File(appDir, exeName);
        FileUtils.copyFile(bundleExe, exeOut);

        // Copy debug symbols if they were generated
        String zipDir = FilenameUtils.concat(project.getBinaryOutputDirectory(), platform.getExtenderPair());
        File bundlePdb = new File(zipDir, "dmengine.pdb");
        if (bundlePdb.exists()) {
            File pdbOut = new File(appDir, "dmengine.pdb");
            FileUtils.copyFile(bundlePdb, pdbOut);
        }

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

        BundleHelper.moveBundleIfNeed(project, bundleDir);
    }
}
