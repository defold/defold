package com.dynamo.bob.bundle;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
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

    private void copyIcon(BobProjectProperties projectProperties, String projectRoot, File appDir, String name, String outName)
            throws IOException {
        String resource = projectProperties.getStringValue("ios", name);
        if (resource != null && resource.length() > 0) {
            File inFile = new File(projectRoot, resource);
            File outFile = new File(appDir, outName);
            FileUtils.copyFile(inFile, outFile);
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
    public void bundleApplication(Project project, File bundleDir)
            throws IOException, CompileExceptionError {
        logger.log(Level.INFO, "Entering IOSBundler.bundleApplication()");

        String tmpPlatform = project.option("platform", null);
        boolean simulatorBinary = tmpPlatform != null && tmpPlatform.equals("x86_64-ios");

        Map<String, IResource> bundleResources = null;
        if (simulatorBinary) {
            bundleResources = ExtenderUtil.collectResources(project, Platform.X86_64Ios);
        } else {
            // Collect bundle/package resources to be included in .App directory
            bundleResources = ExtenderUtil.collectResources(project, Platform.Arm64Darwin);
        }

        boolean debug = project.hasOption("debug");

        File exeArmv7 = null;
        File exeArm64 = null;
        File exeSim64 = null;

        // If a custom engine was built we need to copy it
        String extenderExeDir = FilenameUtils.concat(project.getRootDirectory(), "build");

        if (simulatorBinary)
        {
            // sim64 exe
            {
                Platform targetPlatform = Platform.X86_64Ios;
                File extenderExe = new File(FilenameUtils.concat(extenderExeDir, FilenameUtils.concat(targetPlatform.getExtenderPair(), targetPlatform.formatBinaryName("dmengine"))));
                File defaultExe = new File(Bob.getDmengineExe(targetPlatform, debug));
                exeSim64 = defaultExe;
                if (extenderExe.exists()) {
                    logger.log(Level.INFO, "Using extender exe for sim64");
                    exeSim64 = extenderExe;
                }
            }

            logger.log(Level.INFO, "Sim64 exe: " + getFileDescription(exeSim64));

        } else {
            // armv7 exe
            {
                Platform targetPlatform = Platform.Armv7Darwin;
                File extenderExe = new File(FilenameUtils.concat(extenderExeDir, FilenameUtils.concat(targetPlatform.getExtenderPair(), targetPlatform.formatBinaryName("dmengine"))));
                File defaultExe = new File(Bob.getDmengineExe(targetPlatform, debug));
                exeArmv7 = defaultExe;
                if (extenderExe.exists()) {
                    logger.log(Level.INFO, "Using extender exe for Armv7");
                    exeArmv7 = extenderExe;
                }
            }

            // arm64 exe
            {
                Platform targetPlatform = Platform.Arm64Darwin;
                File extenderExe = new File(FilenameUtils.concat(extenderExeDir, FilenameUtils.concat(targetPlatform.getExtenderPair(), targetPlatform.formatBinaryName("dmengine"))));
                File defaultExe = new File(Bob.getDmengineExe(targetPlatform, debug));
                exeArm64 = defaultExe;
                if (extenderExe.exists()) {
                    logger.log(Level.INFO, "Using extender exe for Arm64");
                    exeArm64 = extenderExe;
                }
            }

            logger.log(Level.INFO, "Armv7 exe: " + getFileDescription(exeArmv7));
            logger.log(Level.INFO, "Arm64 exe: " + getFileDescription(exeArm64));
        }

        BobProjectProperties projectProperties = project.getProjectProperties();
        String title = projectProperties.getStringValue("project", "title", "Unnamed");

        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());
        File appDir = new File(bundleDir, title + ".app");
        logger.log(Level.INFO, "Bundling to " + appDir.getPath());

        String provisioningProfile = project.option("mobileprovisioning", "");
        String identity = project.option("identity", "");

        String projectRoot = project.getRootDirectory();

        FileUtils.deleteDirectory(appDir);
        appDir.mkdirs();

        final boolean useArchive = true;

        if (useArchive) {
            // Copy archive and game.projectc
            for (String name : Arrays.asList("game.projectc", "game.arci", "game.arcd", "game.dmanifest", "game.public.der")) {
                FileUtils.copyFile(new File(buildDir, name), new File(appDir, name));
            }
        } else {
            FileUtils.copyDirectory(buildDir, appDir);
            new File(buildDir, "game.arci").delete();
            new File(buildDir, "game.arcd").delete();
        }

        // Copy icons
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_57x57", "Icon.png");
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_114x114", "Icon@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_72x72", "Icon-72.png");
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_144x144", "Icon-72@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_76x76", "Icon-76.png");
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_152x152", "Icon-76@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_120x120", "Icon-60@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_180x180", "Icon-60@3x.png");
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_167x167", "Icon-167.png");

        // Copy launch images
        // iphone 3, 4, 5 portrait
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_320x480", "Default.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_640x960", "Default@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_640x1136", "Default-568h@2x.png");

        // ipad portrait+landscape
        // backward compatibility with old game.project files with the incorrect launch image sizes
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_768x1004", "Default-Portrait-1024h.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_768x1024", "Default-Portrait-1024h.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_1024x748", "Default-Landscape-1024h.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_1024x768", "Default-Landscape-1024h.png");

        // iPhone 6, 7 and 8 (portrait+landscape)
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_750x1334", "Default-Portrait-667h@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_1334x750", "Default-Landscape-667h@2x.png");

        // iPhone 6 plus portrait+landscape
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_1242x2208", "Default-Portrait-736h@3x.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_2208x1242", "Default-Landscape-736h@3x.png");

        // iPad retina portrait+landscape
        // backward compatibility with old game.project files with the incorrect launch image sizes
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_1536x2008", "Default-Portrait-1024h@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_1536x2048", "Default-Portrait-1024h@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_2048x1496", "Default-Landscape-1024h@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_2048x1536", "Default-Landscape-1024h@2x.png");

        // iPad pro (12")
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_2048x2732", "Default-Portrait-1366h@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_2732x2048", "Default-Landscape-1366h@2x.png");

        // iPhone X (portrait+landscape)
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_1125x2436", "Default-Portrait-812h@3x.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_2436x1125", "Default-Landscape-812h@3x.png");

        List<String> applicationQueriesSchemes = new ArrayList<String>();
        List<String> urlSchemes = new ArrayList<String>();

        String facebookAppId = projectProperties.getStringValue("facebook", "appid", null);
        if (facebookAppId != null) {
            urlSchemes.add("fb" + facebookAppId);

            applicationQueriesSchemes.add("fbapi");
            applicationQueriesSchemes.add("fbapi20130214");
            applicationQueriesSchemes.add("fbapi20130410");
            applicationQueriesSchemes.add("fbapi20130702");
            applicationQueriesSchemes.add("fbapi20131010");
            applicationQueriesSchemes.add("fbapi20131219");
            applicationQueriesSchemes.add("fbapi20140410");
            applicationQueriesSchemes.add("fbapi20140116");
            applicationQueriesSchemes.add("fbapi20150313");
            applicationQueriesSchemes.add("fbapi20150629");
            applicationQueriesSchemes.add("fbauth");
            applicationQueriesSchemes.add("fbauth2");
            applicationQueriesSchemes.add("fb-messenger-api20140430");
            applicationQueriesSchemes.add("fb-messenger-platform-20150128");
            applicationQueriesSchemes.add("fb-messenger-platform-20150218");
            applicationQueriesSchemes.add("fb-messenger-platform-20150305");
        }

        String bundleId = projectProperties.getStringValue("ios", "bundle_identifier");
        if (bundleId != null) {
            urlSchemes.add(bundleId);
        }

        Map<String, Object> properties = new HashMap<String, Object>();
        properties.put("url-schemes", urlSchemes);
        properties.put("application-queries-schemes", applicationQueriesSchemes);

        List<String> orientationSupport = new ArrayList<String>();
        if(projectProperties.getBooleanValue("display", "dynamic_orientation", false)==false) {
            Integer displayWidth = projectProperties.getIntValue("display", "width", 960);
            Integer displayHeight = projectProperties.getIntValue("display", "height", 640);
            if((displayWidth != null & displayHeight != null) && (displayWidth > displayHeight)) {
                orientationSupport.add("LandscapeRight");
            } else {
                orientationSupport.add("Portrait");
            }
        } else {
            orientationSupport.add("Portrait");
            orientationSupport.add("PortraitUpsideDown");
            orientationSupport.add("LandscapeLeft");
            orientationSupport.add("LandscapeRight");
        }
        properties.put("orientation-support", orientationSupport);

        BundleHelper helper = new BundleHelper(project, Platform.Armv7Darwin, bundleDir, ".app");
        helper.format(properties, "ios", "infoplist", new File(appDir, "Info.plist"));

        // Copy bundle resources into .app folder
        ExtenderUtil.writeResourcesToDirectory(bundleResources, appDir);

        // Copy Provisioning Profile
        File provisioningProfileFile = new File(provisioningProfile);
        if (!provisioningProfileFile.exists()) {
            throw new IOException(String.format("You must specify a valid provisioning profile '%s'", provisioningProfile.length() == 0 ? "" : provisioningProfileFile.getAbsolutePath()));
        }
        FileUtils.copyFile(provisioningProfileFile, new File(appDir, "embedded.mobileprovision"));

        // Create fat/universal binary
        File tmpFile = File.createTempFile("dmengine", "");
        tmpFile.deleteOnExit();
        String exe = tmpFile.getPath();

        // Run lipo to add exeArmv7 + exeArm64 together into universal bin
        if (simulatorBinary)
        {
            FileUtils.copyFile(exeSim64, new File(exe));
        } else {
            Result lipoResult = Exec.execResult( Bob.getExe(Platform.getHostPlatform(), "lipo"), "-create", exeArmv7.getAbsolutePath(), exeArm64.getAbsolutePath(), "-output", exe );
            if (lipoResult.ret == 0) {
                logger.log(Level.INFO, "Universal binary: " + getFileDescription(tmpFile));
            }
            else {
                logger.log(Level.SEVERE, "Error executing lipo command:\n" + new String(lipoResult.stdOutErr));
            }
        }

        // Strip executable
        if( !debug )
        {
            Result stripResult = Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "strip_ios"), exe);
            if (stripResult.ret == 0) {
                logger.log(Level.INFO, "Stripped binary: " + getFileDescription(tmpFile));
            }
            else {
                logger.log(Level.SEVERE, "Error executing strip_ios command:\n" + new String(stripResult.stdOutErr));
            }
        }

        // Copy Executable
        File destExecutable = new File(appDir, title);
        FileUtils.copyFile(new File(exe), destExecutable);
        destExecutable.setExecutable(true);
        logger.log(Level.INFO, "Bundle binary: " + getFileDescription(destExecutable));

        // Sign (only if identity and provisioning profile set)
        // iOS simulator can install non signed apps
        if (identity != null && provisioningProfile != null && !identity.isEmpty() && !provisioningProfile.isEmpty()) {
            // Copy Provisioning Profile
            FileUtils.copyFile(new File(provisioningProfile), new File(appDir, "embedded.mobileprovision"));

            File textProvisionFile = File.createTempFile("mobileprovision", ".plist");
            textProvisionFile.deleteOnExit();

            Result securityResult = Exec.execResult("security", "cms", "-D", "-i", provisioningProfile, "-o", textProvisionFile.getAbsolutePath());
            if (securityResult.ret != 0) {
                logger.log(Level.SEVERE, "Error executing security command:\n" + new String(securityResult.stdOutErr));
            }

            File entitlementOut = File.createTempFile("entitlement", ".xcent");

            try {
                XMLPropertyListConfiguration decodedProvision = new XMLPropertyListConfiguration();
                decodedProvision.load(textProvisionFile);
                XMLPropertyListConfiguration entitlements = new XMLPropertyListConfiguration();
                entitlements.append(decodedProvision.configurationAt("Entitlements"));
                entitlementOut.deleteOnExit();
                entitlements.save(entitlementOut);
            } catch (ConfigurationException e) {
                logger.log(Level.SEVERE, "Error reading provisioning profile '" + provisioningProfile + "'. Make sure this is a valid provisioning profile file." );
                throw new RuntimeException(e);
            }

            ProcessBuilder processBuilder = new ProcessBuilder("codesign",
                    "-f", "-s", identity,
                    "--entitlements", entitlementOut.getAbsolutePath(),
                    appDir.getAbsolutePath());
            processBuilder.environment().put("EMBEDDED_PROFILE_NAME", "embedded.mobileprovision");
            processBuilder.environment().put("CODESIGN_ALLOCATE", Bob.getExe(Platform.getHostPlatform(), "codesign_allocate"));

            Process process = processBuilder.start();
            logProcess(process);
        }

        // Package into IPA zip

        // Package zip file
        File tmpZipDir = createTempDirectory();
        tmpZipDir.deleteOnExit();

        // NOTE: We replaced the java zip file implementation(s) due to the fact that XCode didn't want
        // to import the resulting zip files.
        File payloadDir = new File(tmpZipDir, "Payload");
        payloadDir.mkdir();

        ProcessBuilder processBuilder = new ProcessBuilder("cp", "-r", appDir.getAbsolutePath(), payloadDir.getAbsolutePath());
        Process process = processBuilder.start();
        logProcess(process);

        File zipFile = new File(bundleDir, title + ".ipa");
        File zipFileTmp = new File(bundleDir, title + ".ipa.tmp");
        processBuilder = new ProcessBuilder("zip", "-qr", zipFileTmp.getAbsolutePath(), payloadDir.getName());
        processBuilder.directory(tmpZipDir);

        process = processBuilder.start();
        logProcess(process);

        Files.move( Paths.get(zipFileTmp.getAbsolutePath()), Paths.get(zipFile.getAbsolutePath()), StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
        logger.log(Level.INFO, "Finished ipa: " + getFileDescription(zipFile));
    }
}
