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
import org.apache.commons.io.IOUtils;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.Exec;

import com.defold.extender.client.ExtenderClient;

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

    @Override
    public void bundleApplication(Project project, File bundleDir)
            throws IOException, CompileExceptionError {

    	boolean debug = project.hasOption("debug");

    	File root = new File(project.getRootDirectory());
    	boolean hasNativeExtensions = ExtenderClient.hasExtensions(root);

    	File exeArmv7 = null;
    	File exeArm64 = null;

        if (hasNativeExtensions) {
            String platform64 = "arm64-ios";
            String platformv7 = "armv7-ios";

            String sdkVersion = project.option("defoldsdk", "");
            String buildServer = project.option("build-server", "");
            ExtenderClient extender = new ExtenderClient(buildServer);
            File logFile = File.createTempFile("build_" + sdkVersion, ".txt");
            logFile.deleteOnExit();

            exeArm64 = File.createTempFile("engine_" + sdkVersion + "_" + platform64, "");
            exeArm64.deleteOnExit();

            exeArmv7 = File.createTempFile("engine_" + sdkVersion + "_" + platformv7, "");
            exeArmv7.deleteOnExit();

            List<File> allSource = ExtenderClient.getExtensionSource(root, platform64);
            BundleHelper.buildEngineRemote(extender, platform64, sdkVersion, root, allSource, logFile, exeArm64);

            allSource = ExtenderClient.getExtensionSource(root, platformv7);
            BundleHelper.buildEngineRemote(extender, platformv7, sdkVersion, root, allSource, logFile, exeArmv7);
        }
        else
        {
            exeArmv7 = new File( Bob.getDmengineExe(Platform.Armv7Darwin, debug) );
            exeArm64 = new File( Bob.getDmengineExe(Platform.Arm64Darwin, debug) );
        }

        BobProjectProperties projectProperties = project.getProjectProperties();
        String title = projectProperties.getStringValue("project", "title", "Unnamed");

        File buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());
        File appDir = new File(bundleDir, title + ".app");

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
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_1024x748", "Default-Landscape.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_1024x768", "Default-Landscape.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_768x1004", "Default-Portrait.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_768x1024", "Default-Portrait.png");

        // iphone 6
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_750x1334", "Default-667h@2x.png");

        // iphone 6 plus portrait+landscape
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_1242x2208", "Default-Portrait-736h@3x.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_2208x1242", "Default-Landscape-736h@3x.png");

        // ipad retina portrait+landscape
        // backward compatibility with old game.project files with the incorrect launch image sizes
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_1536x2008", "Default-Portrait@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_1536x2048", "Default-Portrait@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_2048x1496", "Default-Landscape@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_2048x1536", "Default-Landscape@2x.png");

        // ipad pro (12")
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_2048x2732", "Default-Portrait-1366h@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "launch_image_2732x2048", "Default-Landscape-1366h@2x.png");

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
            Integer displayWidth = projectProperties.getIntValue("display", "width");
            Integer displayHeight = projectProperties.getIntValue("display", "height");
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
        helper.format(properties, "ios", "infoplist", "resources/ios/Info.plist", new File(appDir, "Info.plist"));

        // Copy Provisioning Profile
        FileUtils.copyFile(new File(provisioningProfile), new File(appDir, "embedded.mobileprovision"));

        // Create fat/universal binary
        File tmpFile = File.createTempFile("dmengine", "");
        tmpFile.deleteOnExit();
        String exe = tmpFile.getPath();

        // Run lipo to add exeArmv7 + exeArm64 together into universal bin
        Exec.exec( Bob.getExe(Platform.getHostPlatform(), "lipo"), "-create", exeArmv7.getAbsolutePath(), exeArm64.getAbsolutePath(), "-output", exe );

        // Strip executable
        if( !debug )
        {
            Exec.execResult(Bob.getExe(Platform.getHostPlatform(), "strip_ios"), exe);
        }

        // Copy Executable
        File destExecutable = new File(appDir, title);
        FileUtils.copyFile(new File(exe), destExecutable);
        destExecutable.setExecutable(true);

        // Sign
        if (identity != null && provisioningProfile != null) {
            File textProvisionFile = File.createTempFile("mobileprovision", ".plist");
            textProvisionFile.deleteOnExit();

            Exec.exec("security", "cms", "-D", "-i", provisioningProfile, "-o", textProvisionFile.getAbsolutePath());

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

            // Package zip file
            File tmpZipDir = createTempDirectory();
            tmpZipDir.deleteOnExit();

            // NOTE: We replaced the java zip file implementation(s) due to the fact that XCode didn't want
            // to import the resulting zip files.
            File payloadDir = new File(tmpZipDir, "Payload");
            payloadDir.mkdir();

            processBuilder = new ProcessBuilder("cp", "-r", appDir.getAbsolutePath(), payloadDir.getAbsolutePath());
            process = processBuilder.start();
            logProcess(process);

            File zipFile = new File(bundleDir, title + ".ipa");
            File zipFileTmp = new File(bundleDir, title + ".ipa.tmp");
            processBuilder = new ProcessBuilder("zip", "-qr", zipFileTmp.getAbsolutePath(), payloadDir.getName());
            processBuilder.directory(tmpZipDir);

            process = processBuilder.start();
            logProcess(process);

            Files.move( Paths.get(zipFileTmp.getAbsolutePath()), Paths.get(zipFile.getAbsolutePath()), StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
        }
    }
}
