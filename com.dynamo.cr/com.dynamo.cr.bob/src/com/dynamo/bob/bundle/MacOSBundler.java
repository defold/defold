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
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.FileUtil;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;
import org.apache.commons.io.FilenameUtils;


@BundlerParams(platforms = {"x86_64-macos", "arm64-macos"})
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

        String bundleIdentifier = project.getProjectProperties().getStringValue("osx", "bundle_identifier");
        if (bundleIdentifier == null) {
            throw new CompileExceptionError("No value for 'osx.bundle_identifier' set in game.project");
        }

        if (!BundleHelper.isValidAppleBundleIdentifier(bundleIdentifier)) {
            throw new CompileExceptionError("macOS bundle identifier '" + bundleIdentifier + "' is not valid.");
        }

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

        BundleHelper.throwIfCanceled(canceled);

        List<File> binaries = IOSBundler.getBinariesFromArchitectures(project, architectures, variant);

        BundleHelper.throwIfCanceled(canceled);

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();
        contentsDir.mkdirs();
        resourcesDir.mkdirs();
        macosDir.mkdirs();

        BundleHelper helper = new BundleHelper(project, platform, bundleDir, variant, this);

        BundleHelper.throwIfCanceled(canceled);

        // Collect bundle/package resources to be included in .App directory
        Map<String, IResource> bundleResources = ExtenderUtil.collectBundleResources(project, architectures);

        BundleHelper.throwIfCanceled(canceled);

        // Copy bundle resources into .app folder
        ExtenderUtil.writeResourcesToDirectory(bundleResources, appDir);

        BundleHelper.throwIfCanceled(canceled);

        if (BundleHelper.isArchiveIncluded(project)) {
            // Copy archive and game.projectc
            for (String name : BundleHelper.getArchiveFilenames(buildDir)) {
                FileUtils.copyFile(new File(buildDir, name), new File(resourcesDir, name));
            }
        }

        BundleHelper.throwIfCanceled(canceled);

        helper.copyOrWriteManifestFile(architectures.get(0), appDir);

        BundleHelper.throwIfCanceled(canceled);

        // Copy icon
        copyIcon(projectProperties, new File(project.getRootDirectory()), resourcesDir);

        // Create fat/universal binary
        File exe = File.createTempFile("dmengine", "");
        FileUtil.deleteOnExit(exe);

        BundleHelper.throwIfCanceled(canceled);

        // Run lipo on supplied architecture binaries.
        BundleHelper.lipoBinaries(exe, binaries);

        BundleHelper.throwIfCanceled(canceled);

        if (strip_executable) {
            BundleHelper.stripExecutable(exe);
        }

        // Copy Executable
        File destExecutable = new File(macosDir, exeName);
        FileUtils.copyFile(exe, destExecutable);
        destExecutable.setExecutable(true);
        logger.info("Bundle binary: " + BundleHelper.getFileDescription(destExecutable));

        if (architectures.size() == 1) {
            File binaryDir = new File(FilenameUtils.concat(project.getBinaryOutputDirectory(), platform.getExtenderPair()));
            BundleHelper.copySharedLibraries(platform, binaryDir, macosDir);
        } else {
            BundleHelper.createFatLibrary(architectures, project.getBinaryOutputDirectory(), macosDir, canceled);
        }

        // Copy debug symbols
        // Create list of dSYM binaries
        File extenderBuildDir = new File(project.getRootDirectory(), "build");
        List<File> symbolDirectories = IOSBundler.getSymbolDirsFromArchitectures(extenderBuildDir, architectures);
        if (symbolDirectories.size() > 0)
        {
            File bundleSymbolsDir = new File(bundleDir, String.format("%s.dSYM", title));
            IOSBundler.generateSymbols(bundleSymbolsDir, exeName, symbolDirectories);
        }

        BundleHelper.throwIfCanceled(canceled);

        if (variant.equals(Bob.VARIANT_DEBUG))
        {
            logger.info("Adding debug entitlements");
            File entitlementsFile = BundleHelper.copyResourceToTempFile("resources/macos/entitlements-debug.plist");
            Result r = Exec.execResult("codesign", "-f", "-s", "-", "--entitlements", entitlementsFile.getAbsolutePath(), appDir.getAbsolutePath());
            if (r.ret != 0) {
                throw new IOException(new String(r.stdOutErr));
            }
        }

        BundleHelper.throwIfCanceled(canceled);

        // Copy PrivacyManifest.xcprivacy
        // According to https://developer.apple.com/documentation/bundleresources/privacy_manifest_files/adding_a_privacy_manifest_to_your_app_or_third-party_sdk#4336738
        // the PrivacyInfo.xcprivacy  on macos should be in `Resources` folder
        BundleHelper.copyPrivacyManifest(project, platform, resourcesDir);

        BundleHelper.moveBundleIfNeed(project, bundleDir);
    }
}
