package com.dynamo.bob.bundle;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.StringWriter;
import java.io.OutputStream;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.configuration.plist.XMLPropertyListConfiguration;
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
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;

public class IOSBundler implements IBundler {
    private static Logger logger = Logger.getLogger(IOSBundler.class.getName());

    private void copyImage(Project project, BobProjectProperties projectProperties, String projectRoot, File appDir, String name, String outName)
            throws IOException {
        String resource = projectProperties.getStringValue("ios", name);
        if (resource != null && resource.length() > 0) {
            IResource inResource = project.getResource(resource);
            if (!inResource.exists()) {
                throw new IOException(String.format("%s does not exist.", resource));
            }
            File outFile = new File(appDir, outName);
            FileUtils.writeByteArrayToFile(outFile, inResource.getContent());
        }
    }

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
                logger.log(Level.SEVERE, errorMessage);
                throw new IOException(errorMessage);
            }
        } catch (InterruptedException e1) {
            throw new RuntimeException(e1);
        }
    }

    private static File createTempDirectory() throws IOException {
        final File temp;

        temp = File.createTempFile("temp", Long.toString(System.nanoTime()));

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

    private String getFileDescription(File file) {
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

    @Override
    public void bundleApplication(Project project, File bundleDir, ICanceled canceled)
            throws IOException, CompileExceptionError {
        logger.log(Level.INFO, "Entering IOSBundler.bundleApplication()");

        BundleHelper.throwIfCanceled(canceled);

        final Platform platform = Platform.Armv7Darwin;
        final List<Platform> architectures = Platform.getArchitecturesFromString(project.option("architectures", ""), platform);

        Map<String, IResource> bundleResources = ExtenderUtil.collectBundleResources(project, architectures);

        final String variant = project.option("variant", Bob.VARIANT_RELEASE);
        final boolean strip_executable = project.hasOption("strip-executable");

        // If a custom engine was built we need to copy it
        boolean hasExtensions = ExtenderUtil.hasNativeExtensions(project);
        File extenderPlatformDir = new File(project.getBuildDirectory(), architectures.get(0).getExtenderPair());
        String extenderExeDir = FilenameUtils.concat(project.getRootDirectory(), "build");

        // Loop over all architectures needed for bundling
        // Pickup each binary, either vanilla or from a extender build.
        List<File> binaries = new ArrayList<File>();
        for (Platform architecture : architectures) {
            List<File> bins = Bob.getNativeExtensionEngineBinaries(architecture, extenderExeDir);
            if (bins == null) {
                bins = Bob.getDefaultDmengineFiles(architecture, variant);
            } else {
                logger.log(Level.INFO, "Using extender binary for " + architecture.getPair());
            }

            File binary = bins.get(0);
            logger.log(Level.INFO, architecture.getPair() + " exe: " + getFileDescription(binary));
            binaries.add(binary);

            BundleHelper.throwIfCanceled(canceled);
        }

        BundleHelper.throwIfCanceled(canceled);
        BobProjectProperties projectProperties = project.getProjectProperties();
        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        String exeName = BundleHelper.projectNameToBinaryName(title);

        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());
        File appDir = new File(bundleDir, title + ".app");
        logger.log(Level.INFO, "Bundling to " + appDir.getPath());

        String provisioningProfile = project.option("mobileprovisioning", null);
        String identity = project.option("identity", null);
        Boolean shouldSign = provisioningProfile != null && identity != null;

        // Verify that the user supplied both of the needed arguments if the application should be signed.
        if (shouldSign) {
            if (provisioningProfile == null) {
                throw new IOException("Cannot sign application without a provisioning profile, missing --mobileprovisioning argument.");
            } else if (identity == null) {
                throw new IOException("Cannot sign application without a signing identity, missing --identity argument.");
            }
        }

        if (shouldSign) {
            logger.log(Level.INFO, "Code signing enabled.");
        } else {
            logger.log(Level.INFO, "Code signing disabled.");
        }

        String projectRoot = project.getRootDirectory();

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();

        BundleHelper.throwIfCanceled(canceled);

        // Copy archive and game.projectc
        for (String name : Arrays.asList("game.projectc", "game.arci", "game.arcd", "game.dmanifest", "game.public.der")) {
            FileUtils.copyFile(new File(buildDir, name), new File(appDir, name));
        }

        BundleHelper.throwIfCanceled(canceled);

        // Copy launch images
        try
        {
            // iphone 3, 4, 5 portrait
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_320x480", "Default.png");
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_640x960", "Default@2x.png");
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_640x1136", "Default-568h@2x.png");

            // ipad portrait+landscape
            // backward compatibility with old game.project files with the incorrect launch image sizes
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_768x1004", "Default-Portrait-1024h.png");
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_768x1024", "Default-Portrait-1024h.png");
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_1024x748", "Default-Landscape-1024h.png");
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_1024x768", "Default-Landscape-1024h.png");

            // iPhone 6, 7 and 8 (portrait+landscape)
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_750x1334", "Default-Portrait-667h@2x.png");
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_1334x750", "Default-Landscape-667h@2x.png");

            // iPhone 6 plus portrait+landscape
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_1242x2208", "Default-Portrait-736h@3x.png");
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_2208x1242", "Default-Landscape-736h@3x.png");

            // iPad retina portrait+landscape
            // backward compatibility with old game.project files with the incorrect launch image sizes
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_1536x2008", "Default-Portrait-1024h@2x.png");
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_1536x2048", "Default-Portrait-1024h@2x.png");
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_2048x1496", "Default-Landscape-1024h@2x.png");
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_2048x1536", "Default-Landscape-1024h@2x.png");

            // iPad pro (10.5")
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_1668x2224", "Default-Portrait-1112h@2x.png");
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_2224x1668", "Default-Landscape-1112h@2x.png");

            // iPad pro (12.9")
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_2048x2732", "Default-Portrait-1366h@2x.png");
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_2732x2048", "Default-Landscape-1366h@2x.png");

            // iPad pro (11")
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_1668x2388", "Default-Portrait-1194h@2x.png");
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_2388x1668", "Default-Landscape-1194h@2x.png");

            // iPhone X, XS (portrait+landscape)
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_1125x2436", "Default-Portrait-812h@3x.png");
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_2436x1125", "Default-Landscape-812h@3x.png");

            // iPhone XR (portrait+landscape
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_828x1792", "Default-Portrait-896h@2x.png");
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_1792x828", "Default-Landscape-896h@2x.png");

            // iPhone XS Max (portrait+landscape)
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_1242x2688", "Default-Portrait-896h@3x.png");
            copyImage(project, projectProperties, projectRoot, appDir, "launch_image_2688x1242", "Default-Landscape-896h@3x.png");
        } catch (Exception e) {
            throw new CompileExceptionError(project.getGameProjectResource(), -1, e);
        }

        BundleHelper helper = new BundleHelper(project, Platform.Armv7Darwin, bundleDir, variant);

        helper.copyOrWriteManifestFile(architectures.get(0), appDir);
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
        File tmpFile = File.createTempFile("dmengine", "");
        tmpFile.deleteOnExit();
        String exe = tmpFile.getPath();

        BundleHelper.throwIfCanceled(canceled);

        // Run lipo on supplied architecture binaries.
        List<String> lipoArgList = new ArrayList<String>();
        lipoArgList.add(Bob.getExe(Platform.getHostPlatform(), "lipo"));
        lipoArgList.add("-create");
        for (File bin : binaries) {
            lipoArgList.add(bin.getAbsolutePath());
        }
        lipoArgList.add("-output");
        lipoArgList.add(exe);

        Result lipoResult = Exec.execResult( lipoArgList.toArray(new String[0]) );
        if (lipoResult.ret == 0) {
            logger.log(Level.INFO, "Universal binary: " + getFileDescription(tmpFile));
        }
        else {
            logger.log(Level.SEVERE, "Error executing lipo command:\n" + new String(lipoResult.stdOutErr));
        }

        BundleHelper.throwIfCanceled(canceled);
        // Strip executable
        if( strip_executable )
        {
            Result stripResult = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "strip_ios"), exe);
            if (stripResult.ret == 0) {
                logger.log(Level.INFO, "Stripped binary: " + getFileDescription(tmpFile));
            }
            else {
                logger.log(Level.SEVERE, "Error executing strip_ios command:\n" + new String(stripResult.stdOutErr));
            }
        }

        BundleHelper.throwIfCanceled(canceled);

        // Copy Executable
        File destExecutable = new File(appDir, exeName);
        FileUtils.copyFile(new File(exe), destExecutable);
        destExecutable.setExecutable(true);
        logger.log(Level.INFO, "Bundle binary: " + getFileDescription(destExecutable));

        // Copy debug symbols
        String zipDir = FilenameUtils.concat(extenderExeDir, Platform.Armv7Darwin.getExtenderPair());
        File buildSymbols = new File(zipDir, "dmengine.dSYM");
        if (buildSymbols.exists()) {
            String symbolsDir = String.format("%s.dSYM", title);

            File bundleSymbols = new File(bundleDir, symbolsDir);
            FileUtils.copyDirectory(buildSymbols, bundleSymbols);
            // Also rename the executable
            File bundleExeOld = new File(bundleSymbols, FilenameUtils.concat("Contents", FilenameUtils.concat("Resources", FilenameUtils.concat("DWARF", "dmengine"))));
            File symbolExe = new File(bundleExeOld.getParent(), destExecutable.getName());
            bundleExeOld.renameTo(symbolExe);
        }

        // Sign (only if identity and provisioning profile set)
        // iOS simulator can install non signed apps
        if (shouldSign && !identity.isEmpty() && !provisioningProfile.isEmpty()) {
            // Copy Provisioning Profile
            FileUtils.copyFile(new File(provisioningProfile), new File(appDir, "embedded.mobileprovision"));

            File textProvisionFile = File.createTempFile("mobileprovision", ".plist");
            textProvisionFile.deleteOnExit();

            Result securityResult = Exec.execResult("security", "cms", "-D", "-i", provisioningProfile, "-o", textProvisionFile.getAbsolutePath());
            if (securityResult.ret != 0) {
                logger.log(Level.SEVERE, "Error executing security command:\n" + new String(securityResult.stdOutErr));
            }

            File entitlementOut = File.createTempFile("entitlement", ".xcent");
            String customEntitlementsProperty = projectProperties.getStringValue("ios", "entitlements");
            Boolean overrideEntitlementsProperty = projectProperties.getBooleanValue("ios", "override_entitlements", false);

            BundleHelper.throwIfCanceled(canceled);

            // If an override entitlements has been set, use that custom entitlements file directly instead of trying to merge it.
            if (overrideEntitlementsProperty) {
                IResource customEntitlementsResource = project.getResource(customEntitlementsProperty);
                InputStream is = new ByteArrayInputStream(customEntitlementsResource.getContent());
                OutputStream outStream = new FileOutputStream(entitlementOut);
                byte[] buffer = new byte[is.available()];
                is.read(buffer);
                outStream.write(buffer);
            } else {
                try {
                    XMLPropertyListConfiguration customEntitlements = new XMLPropertyListConfiguration();
                    XMLPropertyListConfiguration decodedProvision = new XMLPropertyListConfiguration();

                    decodedProvision.load(textProvisionFile);
                    XMLPropertyListConfiguration entitlements = new XMLPropertyListConfiguration();
                    entitlements.append(decodedProvision.configurationAt("Entitlements"));

                    if (customEntitlementsProperty != null && customEntitlementsProperty.length() > 0) {
                        IResource customEntitlementsResource = project.getResource(customEntitlementsProperty);
                        if (customEntitlementsResource.exists()) {
                            InputStream is = new ByteArrayInputStream(customEntitlementsResource.getContent());
                            customEntitlements.load(is);

                            Iterator<String> keys = customEntitlements.getKeys();
                            while (keys.hasNext()) {
                                String key = keys.next();

                                if (entitlements.getProperty(key) == null) {
                                    logger.log(Level.SEVERE, "No such key found in provisions profile entitlements '" + key + "'.");
                                    throw new IOException("Invalid custom iOS entitlements key '" + key + "'.");
                                }
                                entitlements.clearProperty(key);
                            }
                            entitlements.append(customEntitlements);
                        }
                    }

                    entitlementOut.deleteOnExit();
                    entitlements.save(entitlementOut);
                } catch (ConfigurationException e) {
                    logger.log(Level.SEVERE, "Error reading provisioning profile '" + provisioningProfile + "'. Make sure this is a valid provisioning profile file." );
                    throw new RuntimeException(e);
                } catch (IOException e) {
                    logger.log(Level.SEVERE, "Error merging custom entitlements '" + customEntitlementsProperty +"' with entitlements in provisioning profile. Make sure that custom entitlements has corresponding wildcard entries in the provisioning profile.");
                    throw new RuntimeException(e);
                }
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

        // Package zip file
        File tmpZipDir = createTempDirectory();
        tmpZipDir.deleteOnExit();

        // NOTE: We replaced the java zip file implementation(s) due to the fact that XCode didn't want
        // to import the resulting zip files.
        File payloadDir = new File(tmpZipDir, "Payload");
        payloadDir.mkdir();

        BundleHelper.throwIfCanceled(canceled);
        ProcessBuilder processBuilder = new ProcessBuilder("cp", "-r", appDir.getAbsolutePath(), payloadDir.getAbsolutePath());
        Process process = processBuilder.start();
        logProcess(process);

        BundleHelper.throwIfCanceled(canceled);
        File zipFile = new File(bundleDir, title + ".ipa");
        File zipFileTmp = new File(bundleDir, title + ".ipa.tmp");
        processBuilder = new ProcessBuilder("zip", "-qr", zipFileTmp.getAbsolutePath(), payloadDir.getName());
        processBuilder.directory(tmpZipDir);

        BundleHelper.throwIfCanceled(canceled);
        process = processBuilder.start();
        logProcess(process);

        BundleHelper.throwIfCanceled(canceled);
        Files.move( Paths.get(zipFileTmp.getAbsolutePath()), Paths.get(zipFile.getAbsolutePath()), StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
        logger.log(Level.INFO, "Finished ipa: " + getFileDescription(zipFile));
    }
}
