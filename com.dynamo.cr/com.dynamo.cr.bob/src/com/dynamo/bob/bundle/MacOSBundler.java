// Copyright 2020-2022 The Defold Foundation
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
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;

@BundlerParams(platforms = {Platform.X86_64MacOS})
public class MacOSBundler implements IBundler {
    private static Logger logger = Logger.getLogger(MacOSBundler.class.getName());
    public static final String ICON_NAME = "icon.icns";

    private void copyIcon(BobProjectProperties projectProperties, File projectRoot, File resourcesDir) throws IOException {
        String name = projectProperties.getStringValue("osx", "app_icon");
        if (name != null && name.trim().length() > 0) {
            File inFile = new File(projectRoot, name);
            File outFile = new File(resourcesDir, ICON_NAME);
            FileUtils.copyFile(inFile, outFile);
        }
    }

    private static String MANIFEST_NAME = "Info.plist";

    @Override
    public IResource getManifestResource(Project project, Platform platform) throws IOException {
        return project.getResource("osx", "infoplist");
    }

    @Override
    public String getMainManifestName(Platform platform) {
        return MANIFEST_NAME;
    }

    @Override
    public String getMainManifestTargetPath(Platform platform) {
        // no need for a path here, we dictate where the file is written anyways with copyOrWriteManifestFile
        return "Contents/" + MANIFEST_NAME;
    }

    @Override
    public void updateManifestProperties(Project project, Platform platform,
                                BobProjectProperties projectProperties,
                                Map<String, Map<String, Object>> propertiesMap,
                                Map<String, Object> properties) throws IOException {

        String applicationLocalizationsStr = projectProperties.getStringValue("osx", "localizations", null);
        List<String> applicationLocalizations = BundleHelper.createArrayFromString(applicationLocalizationsStr);
        properties.put("application-localizations", applicationLocalizations);

        String title = projectProperties.getStringValue("project", "title", "dmengine");
        properties.put("bundle-name", projectProperties.getStringValue("osx", "bundle_name", IOSBundler.derivedBundleName(title)));

    }

    @Override
    public void bundleApplication(Project project, Platform platform, File bundleDir, ICanceled canceled)
            throws IOException, CompileExceptionError {

        final List<Platform> architectures = Platform.getArchitecturesFromString(project.option("architectures", ""), platform);

        final String variant = project.option("variant", Bob.VARIANT_RELEASE);
        final boolean strip_executable = project.hasOption("strip-executable");

        BobProjectProperties projectProperties = project.getProjectProperties();
        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        String exeName = BundleHelper.projectNameToBinaryName(title);

        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());
        File appDir = new File(bundleDir, title + ".app");
        File contentsDir = new File(appDir, "Contents");
        File resourcesDir = new File(contentsDir, "Resources");
        File macosDir = new File(contentsDir, "MacOS");

        String extenderExeDir = FilenameUtils.concat(project.getRootDirectory(), "build");

        BundleHelper.throwIfCanceled(canceled);

        List<File> bundleExes = Bob.getNativeExtensionEngineBinaries(platform, extenderExeDir);
        if (bundleExes == null) {
            bundleExes = Bob.getDefaultDmengineFiles(platform, variant);
        }
        if (bundleExes.size() > 1) {
            throw new IOException("Invalid number of binaries for macOS when bundling: " + bundleExes.size());
        }
        File bundleExe = bundleExes.get(0);

        BundleHelper.throwIfCanceled(canceled);

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();
        contentsDir.mkdirs();
        resourcesDir.mkdirs();
        macosDir.mkdirs();

        BundleHelper helper = new BundleHelper(project, platform, bundleDir, variant);

        BundleHelper.throwIfCanceled(canceled);

        // Collect bundle/package resources to be included in .App directory
        Map<String, IResource> bundleResources = ExtenderUtil.collectBundleResources(project, architectures);

        BundleHelper.throwIfCanceled(canceled);

        // Copy bundle resources into .app folder
        ExtenderUtil.writeResourcesToDirectory(bundleResources, appDir);

        BundleHelper.throwIfCanceled(canceled);

        // Copy archive and game.projectc
        for (String name : BundleHelper.getArchiveFilenames(buildDir)) {
            FileUtils.copyFile(new File(buildDir, name), new File(resourcesDir, name));
        }

        BundleHelper.throwIfCanceled(canceled);

        helper.copyOrWriteManifestFile(platform, appDir);

        BundleHelper.throwIfCanceled(canceled);

        // Copy icon
        copyIcon(projectProperties, new File(project.getRootDirectory()), resourcesDir);

        // Copy Executable
        File exeOut = new File(macosDir, exeName);
        FileUtils.copyFile(bundleExe, exeOut);
        exeOut.setExecutable(true);

        // Copy debug symbols
        String zipDir = FilenameUtils.concat(extenderExeDir, platform.getExtenderPair());
        File buildSymbols = new File(zipDir, "dmengine.dSYM");
        if (buildSymbols.exists()) {
            String symbolsDir = String.format("%s.dSYM", title);

            File bundleSymbols = new File(bundleDir, symbolsDir);
            FileUtils.copyDirectory(buildSymbols, bundleSymbols);
            // Also rename the executable
            File bundleExeOld = new File(bundleSymbols, FilenameUtils.concat("Contents", FilenameUtils.concat("Resources", FilenameUtils.concat("DWARF", "dmengine"))));
            File symbolExe = new File(bundleExeOld.getParent(), exeOut.getName());
            bundleExeOld.renameTo(symbolExe);
        }

        BundleHelper.throwIfCanceled(canceled);
        // Strip executable
        if( strip_executable )
        {
            // Currently, we don't have a "strip_darwin.exe" for win32/linux, so we have to pass on those platforms
            if (Platform.getHostPlatform() == Platform.X86_64MacOS) {
                Result stripResult = Exec.execResult(Bob.getExe(platform, "strip_ios"), exeOut.getPath()); // Using the same executable
                if (stripResult.ret != 0) {
                    logger.log(Level.SEVERE, "Error executing strip command:\n" + new String(stripResult.stdOutErr));
                }
            }
        }
    }

}
