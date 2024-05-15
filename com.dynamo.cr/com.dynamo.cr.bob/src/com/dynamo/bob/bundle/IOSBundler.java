// Copyright 2020-2024 The Defold Foundation
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

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import org.apache.commons.configuration2.io.FileLocator;
import org.apache.commons.configuration2.io.FileLocator.FileLocatorBuilder;
import org.apache.commons.configuration2.io.FileLocatorUtils;
import org.apache.commons.configuration2.ex.ConfigurationException;
import org.apache.commons.configuration2.plist.XMLPropertyListConfiguration;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;
import com.dynamo.bob.util.FileUtil;

@BundlerParams(platforms = {Platform.Arm64Ios, Platform.X86_64Ios})
public class IOSBundler implements IBundler {
    private static Logger logger = Logger.getLogger(IOSBundler.class.getName());

    private void logProcess(Process process)
            throws RuntimeException, IOException {
        try {
            InputStream errorIn = process.getErrorStream();
            ByteArrayOutputStream errorOut = new ByteArrayOutputStream();
            IOUtils.copy(errorIn, errorOut);
            errorIn.close();
            String errorMessage = new String(errorOut.toByteArray());

            int ret = process.waitFor();
            if (ret != 0) {
                logger.severe(errorMessage);
                throw new IOException(errorMessage);
            }
        } catch (InterruptedException e1) {
            throw new RuntimeException(e1);
        }
    }

    private static File createTempDirectory() throws IOException {
        final File temp;

        temp = File.createTempFile("temp_defold_", Long.toString(System.nanoTime()));

        if(!(temp.delete()))
        {
            throw new IOException("Could not delete temp file: " + temp.getAbsolutePath());
        }

        if(!(temp.mkdir()))
        {
            throw new IOException("Could not create temp directory: " + temp.getAbsolutePath());
        }

        return (temp);
    }

    public static String getFileDescription(File file) {
        if (file == null) {
            return "null";
        }

        try {
            if (file.isDirectory()) {
                return file.getAbsolutePath() + " (directory)";
            }

            long byteSize = file.length();

            if (byteSize > 0) {
                return file.getAbsolutePath() + " (" + byteSize + " bytes)";
            }

            return file.getAbsolutePath() + " (unknown size)";
        }
        catch (Exception e) {
            // Ignore.
        }

        return file.getPath();
    }

    public static void lipoBinaries(File resultFile, List<File> binaries) throws IOException, CompileExceptionError {
        if (binaries.size() == 1) {
            FileUtils.copyFile(binaries.get(0), resultFile);
            return;
        }

        String exe = resultFile.getPath();
        List<String> lipoArgList = new ArrayList<String>();
        lipoArgList.add(Bob.getExe(Platform.getHostPlatform(), "lipo"));
        lipoArgList.add("-create");
        for (File bin : binaries) {
            lipoArgList.add(bin.getAbsolutePath());
        }
        lipoArgList.add("-output");
        lipoArgList.add(exe);

        Result lipoResult = Exec.execResult(lipoArgList.toArray(new String[0]));
        if (lipoResult.ret == 0) {
            logger.info("Result of lipo command is a universal binary: " + getFileDescription(resultFile));
        }
        else {
            logger.severe("Error executing lipo command:\n" + new String(lipoResult.stdOutErr));
        }
    }

    public static boolean isMacOS(Platform platform) {
        return platform == Platform.X86_64MacOS ||
               platform == Platform.Arm64MacOS;
    }

    public static void stripExecutable(Platform platform, File exe) throws IOException {
        // Currently, we don't have a "strip_darwin.exe" for win32/linux, so we have to pass on those platforms
        if (isMacOS(Platform.getHostPlatform())) {
            String stripName = isMacOS(platform) ? "strip" : "strip_ios";

            Result stripResult = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "strip"), exe.getPath()); // Using the same executable
            if (stripResult.ret != 0) {
                logger.severe("Error executing strip command:\n" + new String(stripResult.stdOutErr));
            }
        }
    }

    public static List<File> getBinariesFromArchitectures(Project project, List<Platform> architectures, String variant) throws IOException {
        // Loop over all architectures needed for bundling
        // Pickup each binary, either vanilla or from a extender build.
        List<File> binaries = new ArrayList<File>();
        for (Platform architecture : architectures) {
            List<File> bins = ExtenderUtil.getNativeExtensionEngineBinaries(project, architecture);
            if (bins == null) {
                bins = Bob.getDefaultDmengineFiles(architecture, variant);
            }
            else {
                logger.info("Using extender binary for " + architecture.getPair());
            }

            File binary = bins.get(0);
            logger.info(architecture.getPair() + " exe: " + getFileDescription(binary));
            binaries.add(binary);
        }
        return binaries;
    }

    private static final String SYMBOL_EXE_RELATIVE_PATH = String.format("Contents/Resources/DWARF/dmengine");

    public static List<File> getSymbolDirsFromArchitectures(File buildDir, List<Platform> architectures) {
        final String[] prefixes = {"", "src" + File.separator};
        List<File> symbolDirectories = new ArrayList<File>();
        for (Platform architecture : architectures) {

            File platformDir = new File(buildDir, architecture.getExtenderPair());
            for (String prefix: prefixes) {
                File symbolsDir = new File(platformDir, prefix + "dmengine.dSYM");
                if (symbolsDir.exists()) {
                    File symbols = new File(symbolsDir, SYMBOL_EXE_RELATIVE_PATH);
                    if (symbols.exists()) {
                        symbolDirectories.add(symbolsDir);
                    }
                }
            }
        }
        return symbolDirectories;
    }

    public static void generateSymbols(File targetFolder, String exeName, List<File> symbolDirectories) throws IOException, CompileExceptionError {
        // For legacy reasons, we use one of the folder to copy extra files from the output architectures
        File symbolsDir = symbolDirectories.get(0);

        // Copy any extra files in the folders (e.g. Info.plist) // TODO: Is this really necessary?
        FileUtils.copyDirectory(symbolsDir, targetFolder);

        // Create the target exe file
        File destSymbolsExeTmp = new File(targetFolder, SYMBOL_EXE_RELATIVE_PATH);
        File destSymbolsExe = new File(destSymbolsExeTmp.getParent(), exeName);

        List<File> dSYMBinaries = new ArrayList<>();
        for (File symbolDir : symbolDirectories) {
            File symbols = new File(symbolDir, SYMBOL_EXE_RELATIVE_PATH);
            if (symbols.exists())
                dSYMBinaries.add(symbols);
        }

        lipoBinaries(destSymbolsExeTmp, dSYMBinaries);
        destSymbolsExeTmp.renameTo(destSymbolsExe);

        logger.info("Symbols binary: " + getFileDescription(destSymbolsExe));
    }

    private static String MANIFEST_NAME = "Info.plist";

    @Override
    public IResource getManifestResource(Project project, Platform platform) throws IOException {
        return project.getResource("ios", "infoplist");
    }

    @Override
    public String getMainManifestName(Platform platform) {
        return MANIFEST_NAME;
    }

    @Override
    public String getMainManifestTargetPath(Platform platform) {
        return MANIFEST_NAME; // Relative path under appDir
    }

    public static String derivedBundleName(String title) {
        return title.substring(0, Math.min(title.length(), 15));
    }

    @Override
    public void updateManifestProperties(Project project, Platform platform,
                                BobProjectProperties projectProperties,
                                Map<String, Map<String, Object>> propertiesMap,
                                Map<String, Object> properties) throws IOException {


        List<String> applicationQueriesSchemes = new ArrayList<String>();
        List<String> urlSchemes = new ArrayList<String>();
        String bundleId = projectProperties.getStringValue("ios", "bundle_identifier");
        if (bundleId != null) {
            urlSchemes.add(bundleId);
        }

        String title = projectProperties.getStringValue("project", "title", "dmengine");

        properties.put("url-schemes", urlSchemes);
        properties.put("application-queries-schemes", applicationQueriesSchemes);
        properties.put("bundle-name", projectProperties.getStringValue("ios", "bundle_name", derivedBundleName(title)));
        properties.put("bundle-version", projectProperties.getStringValue("ios", "bundle_version",
            projectProperties.getStringValue("project", "version", "1.0")
        ));

        String launchScreen = projectProperties.getStringValue("ios", "launch_screen", "LaunchScreen");
        properties.put("launch-screen", FilenameUtils.getBaseName(launchScreen));

        List<String> orientationSupport = new ArrayList<String>();
        if(projectProperties.getBooleanValue("display", "dynamic_orientation", false)==false) {
            Integer displayWidth = projectProperties.getIntValue("display", "width", 960);
            Integer displayHeight = projectProperties.getIntValue("display", "height", 640);
            if((displayWidth != null & displayHeight != null) && (displayWidth > displayHeight)) {
                orientationSupport.add("LandscapeRight");
                orientationSupport.add("LandscapeLeft");
            } else {
                orientationSupport.add("Portrait");
                orientationSupport.add("PortraitUpsideDown");
            }
        } else {
            orientationSupport.add("Portrait");
            orientationSupport.add("PortraitUpsideDown");
            orientationSupport.add("LandscapeLeft");
            orientationSupport.add("LandscapeRight");
        }
        properties.put("orientation-support", orientationSupport);

        String applicationLocalizationsStr = projectProperties.getStringValue("ios", "localizations", null);
        List<String> applicationLocalizations = BundleHelper.createArrayFromString(applicationLocalizationsStr);
        properties.put("application-localizations", applicationLocalizations);

    }

    private void copyManifestFile(BundleHelper helper, Platform platform, File destDir) throws IOException, CompileExceptionError {
        File manifestFile = helper.copyOrWriteManifestFile(platform, destDir);
        String manifest = FileUtils.readFileToString(manifestFile, StandardCharsets.UTF_8);
        // remove attribute definition (https://github.com/defold/defold/pull/6914)
        // it is automatically removed if the manifest was merged
        manifest = manifest.replace("[ <!ATTLIST key merge (keep) #IMPLIED> ]", "");
        FileUtils.write(manifestFile, manifest);
    }

    @Override
    public void bundleApplication(Project project, Platform platform, File bundleDir, ICanceled canceled) throws IOException, CompileExceptionError {
        logger.info("Entering IOSBundler.bundleApplication()");

        String bundleIdentifier = project.getProjectProperties().getStringValue("ios", "bundle_identifier");
        if (bundleIdentifier == null) {
            throw new CompileExceptionError("No value for 'ios.bundle_identifier' set in game.project");
        }

        if (!BundleHelper.isValidAppleBundleIdentifier(bundleIdentifier)) {
            throw new CompileExceptionError("iOS bundle identifier '" + bundleIdentifier + "' is not valid.");
        }

        BundleHelper.throwIfCanceled(canceled);

        final List<Platform> architectures = Platform.getArchitecturesFromString(project.option("architectures", ""), platform);

        Map<String, IResource> bundleResources = ExtenderUtil.collectBundleResources(project, architectures);

        final String variant = project.option("variant", Bob.VARIANT_RELEASE);
        final boolean strip_executable = project.hasOption("strip-executable");

        BundleHelper.throwIfCanceled(canceled);
        BobProjectProperties projectProperties = project.getProjectProperties();
        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        String exeName = BundleHelper.projectNameToBinaryName(title);

        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());
        File appDir = new File(bundleDir, title + ".app");
        File frameworksDir = new File(appDir, "Frameworks");
        logger.info("Bundling to " + appDir.getPath());

        String provisioningProfile = project.option("mobileprovisioning", null);
        String identity = project.option("identity", null);
        Boolean shouldSign = provisioningProfile != null && identity != null;

        // Verify that the user supplied both of the needed arguments if the application should be signed.
        if (shouldSign) {
            if (provisioningProfile == null) {
                throw new IOException("Cannot sign application without a provisioning profile, missing --mobileprovisioning argument.");
            }
            else if (identity == null) {
                throw new IOException("Cannot sign application without a signing identity, missing --identity argument.");
            }
        }

        if (shouldSign) {
            logger.info("Code signing enabled.");
        }
        else {
            logger.info("Code signing disabled.");
        }

        String projectRoot = project.getRootDirectory();

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();
        frameworksDir.mkdirs();

        BundleHelper.throwIfCanceled(canceled);

        if (BundleHelper.isArchiveIncluded(project)) {
            // Copy archive and game.projectc
            for (String name : BundleHelper.getArchiveFilenames(buildDir)) {
                FileUtils.copyFile(new File(buildDir, name), new File(appDir, name));
            }
        }

        BundleHelper.throwIfCanceled(canceled);

        String launchScreen = projectProperties.getStringValue("ios", "launch_screen");
        // It might be null if the user uses the "bundle_resources" to copy everything
        if (launchScreen != null) {
            final String launchScreenBaseName = FilenameUtils.getName(launchScreen);
            IResource source = project.getResource(launchScreen);
            IResource storyboardPlist = project.getResource(launchScreen + "/Info.plist");
            if (source == null) {
                throw new IOException(String.format("'ios.launch_screen' = '%s' does not exist", source));
            }
            // We cannot really if it's a directory, due to our resource abstractions
            // but we can check for a file that should be there, the Info.plist
            if (storyboardPlist == null) {
                throw new IOException(String.format("'ios.launch_screen' = '%s' does not contain an Info.plist", source));
            }

            // Get all files that needs copying
            List<IResource> resources = BundleHelper.listFilesRecursive(project, launchScreen);

            for (IResource r : resources) {

                // Don't create a file out of the source folder name!
                if (r.getPath().equals(source.getPath()))
                    continue;

                String relativePath = r.getPath().substring(source.getPath().length());
                File target = new File(appDir, launchScreenBaseName + "/" + relativePath);

                try {
                    File parentDir = target.getParentFile();
                    if (!parentDir.exists()) {
                        Files.createDirectories(parentDir.toPath());
                    }

                    FileUtils.writeByteArrayToFile(target, r.getContent());
                }
                catch (IOException e) {
                    logger.severe("Failed copying %s to %s\n", r.getPath(), target);
                    throw e;
                }
            }
        } else
        {
            logger.warning("ios.launch_screen is not set");
        }

        String iconsAsset = projectProperties.getStringValue("ios", "icons_asset");
        // It might be null if the user uses the "bundle_resources" to copy everything
        if (iconsAsset != null) {
            final String iconsName = FilenameUtils.getName(iconsAsset);
            IResource source = project.getResource(iconsAsset);
            if (source == null) {
                throw new IOException(String.format("'ios.icons_asset' = '%s' does not exist", source));
            }

            File target = new File(appDir, iconsName);

            try {
                FileUtils.writeByteArrayToFile(target, source.getContent());
            }
            catch (IOException e) {
                logger.severe("Failed copying %s to %s\n", source.getPath(), target);
                throw e;
            }
        }
        else {
            logger.warning("ios.icons_asset is not set");
        }

        BundleHelper helper = new BundleHelper(project, Platform.Arm64Ios, bundleDir, variant, this);
        copyManifestFile(helper, architectures.get(0), appDir);
        helper.copyIosIcons();

        BundleHelper.throwIfCanceled(canceled);
        // Copy bundle resources into .app folder
        ExtenderUtil.writeResourcesToDirectory(bundleResources, appDir);

        BundleHelper.throwIfCanceled(canceled);
        // Copy Provisioning Profile
        if (shouldSign) {
            File provisioningProfileFile = new File(provisioningProfile);
            if (!provisioningProfileFile.exists()) {
                throw new IOException(String.format("You must specify a valid provisioning profile '%s'", provisioningProfile.length() == 0 ? "" : provisioningProfileFile.getAbsolutePath()));
            }
            FileUtils.copyFile(provisioningProfileFile, new File(appDir, "embedded.mobileprovision"));
        }

        BundleHelper.throwIfCanceled(canceled);
        // Create fat/universal binary
        File exe = File.createTempFile("dmengine", "");
        FileUtil.deleteOnExit(exe);

        BundleHelper.throwIfCanceled(canceled);

        // Loop over all architectures needed for bundling
        // Pickup each binary, either vanilla or from a extender build.
        List<File> binaries = getBinariesFromArchitectures(project, architectures, variant);

        // Run lipo on supplied architecture binaries.
        lipoBinaries(exe, binaries);

        BundleHelper.throwIfCanceled(canceled);
        if( strip_executable ) {
            stripExecutable(platform, exe);
        }

        BundleHelper.throwIfCanceled(canceled);

        // Copy Executable
        File destExecutable = new File(appDir, exeName);
        FileUtils.copyFile(exe, destExecutable);
        destExecutable.setExecutable(true);

        // Copy extension frameworks
        for (Platform architecture : architectures) {
            File extensionArchitectureDir = new File(project.getBinaryOutputDirectory(), architecture.getExtenderPair());
            if (!extensionArchitectureDir.exists()) continue;
            File extensionFrameworksDir = new File(extensionArchitectureDir, "frameworks");
            if (!extensionFrameworksDir.exists()) continue;
            for (File extensionFrameworkDir : extensionFrameworksDir.listFiles()) {
                File dest = new File(frameworksDir, extensionFrameworkDir.getName());
                FileUtils.copyDirectory(extensionFrameworkDir, dest);
                logger.fine("Copy framework " + extensionFrameworkDir);
            }
        }

        // Copy debug symbols
        // Create list of dSYM binaries
        File extenderBuildDir = new File(project.getRootDirectory(), "build");
        List<File> symbolDirectories = getSymbolDirsFromArchitectures(extenderBuildDir, architectures);
        if (symbolDirectories.size() > 0)
        {
            File bundleSymbolsDir = new File(bundleDir, String.format("%s.dSYM", title));
            generateSymbols(bundleSymbolsDir, exeName, symbolDirectories);
        }

        BundleHelper.throwIfCanceled(canceled);

        // Copy PrivacyManifest.xcprivacy
        BundleHelper.copyPrivacyManifest(project, platform, appDir);
        BundleHelper.throwIfCanceled(canceled);

        // Package zip file
        File tmpZipDir = createTempDirectory();
        FileUtil.deleteOnExit(tmpZipDir);
        File swiftSupportDir = new File(tmpZipDir, "SwiftSupport");

        // Copy any libswift*.dylib files from the Frameworks folder
        if (frameworksDir.exists()) {
            logger.fine("Copying to /SwiftSupport folder");
            File iphoneosDir = new File(swiftSupportDir, "iphoneos");

            for (File file : frameworksDir.listFiles(File::isFile)) {

                BundleHelper.throwIfCanceled(canceled);

                if (!file.getName().endsWith(".dylib"))
                    continue;

                if (!file.getName().startsWith("libswift")) {
                    continue;
                }

                if (!swiftSupportDir.exists()) {
                    swiftSupportDir.mkdir();
                    iphoneosDir.mkdir();
                }

                File dst = new File(iphoneosDir, file.getName());
                FileUtils.copyFile(file, dst);
                System.out.printf("Copying %s to %s\n", file, dst);
            }
        }

        // Sign (only if identity and provisioning profile set)
        // iOS simulator can install non signed apps
        if (shouldSign && !identity.isEmpty() && !provisioningProfile.isEmpty()) {
            // Copy Provisioning Profile
            FileUtils.copyFile(new File(provisioningProfile), new File(appDir, "embedded.mobileprovision"));

            File textProvisionFile = File.createTempFile("mobileprovision", ".plist");
            FileUtil.deleteOnExit(textProvisionFile);

            Result securityResult = Exec.execResult("security", "cms", "-D", "-i", provisioningProfile, "-o", textProvisionFile.getAbsolutePath());
            if (securityResult.ret != 0) {
                logger.severe("Error executing security command:\n" + new String(securityResult.stdOutErr));
            }

            File entitlementOut = File.createTempFile("entitlement", ".xcent");
            String customEntitlementsProperty = projectProperties.getStringValue("ios", "entitlements");
            Boolean overrideEntitlementsProperty = projectProperties.getBooleanValue("ios", "override_entitlements", false);

            BundleHelper.throwIfCanceled(canceled);

            // If an override entitlements has been set, use that custom entitlements file directly instead of trying to merge it.
            if (overrideEntitlementsProperty) {
                try {
                    if (customEntitlementsProperty == null || customEntitlementsProperty.length() == 0) {
                        throw new IOException("Override Entitlements option was set in game.project but no custom entitlements file were provided.");
                    }
                    IResource customEntitlementsResource = project.getResource(customEntitlementsProperty);
                    InputStream is = new ByteArrayInputStream(customEntitlementsResource.getContent());
                    OutputStream outStream = new FileOutputStream(entitlementOut);
                    byte[] buffer = new byte[is.available()];
                    is.read(buffer);
                    outStream.write(buffer);
                }
                catch (Exception e) {
                    logger.severe("Error when loading custom entitlements from file '" + customEntitlementsProperty + "'.");
                    throw new RuntimeException(e);
                }
            }
            else {
                try {
                    XMLPropertyListConfiguration customEntitlements = new XMLPropertyListConfiguration();
                    XMLPropertyListConfiguration decodedProvision = new XMLPropertyListConfiguration();

                    decodedProvision.read(new FileReader(textProvisionFile));
                    XMLPropertyListConfiguration entitlements = new XMLPropertyListConfiguration();
                    entitlements.append(decodedProvision.configurationAt("Entitlements"));
                    if (customEntitlementsProperty != null && customEntitlementsProperty.length() > 0) {
                        IResource customEntitlementsResource = project.getResource(customEntitlementsProperty);
                        if (customEntitlementsResource.exists()) {
                            InputStream is = new ByteArrayInputStream(customEntitlementsResource.getContent());
                            customEntitlements.read(new InputStreamReader(is));

                            // the custom entitlements file is expected to have the entitlements
                            // in the root <plist><dict>. To offer a bit of flexibility we
                            // also support custom entitlements with an <Entitlements><dict>
                            // property which we extract and put in the root:
                            if (customEntitlements.getKeys("Entitlements").hasNext()) {
                                XMLPropertyListConfiguration tmp = new XMLPropertyListConfiguration();
                                tmp.append(customEntitlements.configurationAt("Entitlements"));
                                customEntitlements = tmp;
                            }

                            Iterator<String> keys = customEntitlements.getKeys();
                            while (keys.hasNext()) {
                                String key = keys.next();

                                if (entitlements.getProperty(key) == null) {
                                    logger.severe("No such key found in provisions profile entitlements '" + key + "'.");
                                    throw new IOException("Invalid custom iOS entitlements key '" + key + "'.");
                                }
                                entitlements.clearProperty(key);
                            }
                            entitlements.append(customEntitlements);
                        }
                    }
                    FileWriter writer = new FileWriter(entitlementOut);
                    FileLocatorBuilder builder = FileLocatorUtils.fileLocator();
                    FileLocator locator = new FileLocator(builder);
                    entitlements.initFileLocator(locator);
                    entitlements.write(writer);
                    writer.close();
                    FileUtil.deleteOnExit(entitlementOut);
                }
                catch (ConfigurationException e) {
                    logger.severe("Error reading provisioning profile '" + provisioningProfile + "'. Make sure this is a valid provisioning profile file." );
                    throw new RuntimeException(e);
                }
                catch (IOException e) {
                    logger.severe("Error merging custom entitlements '" + customEntitlementsProperty +"' with entitlements in provisioning profile. Make sure that custom entitlements has corresponding wildcard entries in the provisioning profile.");
                    throw new RuntimeException(e);
                }
            }

            // Sign any .dylib files in the Frameworks folder
            if (frameworksDir.exists()) {
                logger.info("Signing ./Frameworks folder");
                for (File file : frameworksDir.listFiles()) {

                    BundleHelper.throwIfCanceled(canceled);

                    if (!file.getName().endsWith(".dylib") && !file.getName().endsWith(".framework"))
                        continue;

                    ProcessBuilder processBuilder = new ProcessBuilder("codesign", "-f", "-s", identity, file.getAbsolutePath());
                    processBuilder.environment().put("CODESIGN_ALLOCATE", Bob.getExe(Platform.getHostPlatform(), "codesign_allocate"));

                    Process process = processBuilder.start();
                    logProcess(process);
                }
            }
            else {
                System.out.printf("No ./Framework folder to sign\n");
            }

            // Sign any .appex files in the PlugIns folder
            File pluginsDir = new File(appDir, "PlugIns");
            if (pluginsDir.exists()) {
                logger.info("Signing ./PlugIns folder");
                for (File file : pluginsDir.listFiles()) {

                    BundleHelper.throwIfCanceled(canceled);

                    if (!file.getName().endsWith(".appex")) {
                        continue;
                    }

                    ProcessBuilder processBuilder = new ProcessBuilder("codesign", "--preserve-metadata=entitlements", "-f", "-s", identity, file.getAbsolutePath());
                    processBuilder.environment().put("CODESIGN_ALLOCATE", Bob.getExe(Platform.getHostPlatform(), "codesign_allocate"));

                    Process process = processBuilder.start();
                    logProcess(process);
                }
            }
            else {
                System.out.printf("No ./PlugIns folder to sign\n");
            }


            BundleHelper.throwIfCanceled(canceled);
            ProcessBuilder processBuilder = new ProcessBuilder("codesign",
                    "-f", "-s", identity,
                    "--entitlements", entitlementOut.getAbsolutePath(),
                    appDir.getAbsolutePath());
            processBuilder.environment().put("EMBEDDED_PROFILE_NAME", "embedded.mobileprovision");
            processBuilder.environment().put("CODESIGN_ALLOCATE", Bob.getExe(Platform.getHostPlatform(), "codesign_allocate"));

            BundleHelper.throwIfCanceled(canceled);
            Process process = processBuilder.start();
            logProcess(process);
        }

        // Package into IPA zip
        BundleHelper.throwIfCanceled(canceled);

        File payloadDir = new File(tmpZipDir, "Payload");
        payloadDir.mkdir();

        BundleHelper.throwIfCanceled(canceled);
        ProcessBuilder processBuilder = new ProcessBuilder("cp", "-r", appDir.getAbsolutePath(), payloadDir.getAbsolutePath());
        Process process = processBuilder.start();
        logProcess(process);

        BundleHelper.throwIfCanceled(canceled);
        File zipFile = new File(bundleDir, title + ".ipa");
        File zipFileTmp = new File(bundleDir, title + ".ipa.tmp");

        // NOTE: We replaced the java zip file implementation(s) due to the fact that XCode didn't want
        // to import the resulting zip files.
        if (swiftSupportDir.exists()) {
            processBuilder = new ProcessBuilder("zip", "-qr", zipFileTmp.getAbsolutePath(), payloadDir.getName(), swiftSupportDir.getName());
        }
        else {
            processBuilder = new ProcessBuilder("zip", "-qr", zipFileTmp.getAbsolutePath(), payloadDir.getName());
        }

        processBuilder.directory(tmpZipDir);

        BundleHelper.throwIfCanceled(canceled);
        process = processBuilder.start();
        logProcess(process);

        BundleHelper.throwIfCanceled(canceled);
        Files.move( Paths.get(zipFileTmp.getAbsolutePath()), Paths.get(zipFile.getAbsolutePath()), StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
        logger.info("Finished ipa: " + getFileDescription(zipFile));
    }
}
