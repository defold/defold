package com.dynamo.bob.bundle;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.configuration.plist.XMLPropertyListConfiguration;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.Exec;

public class TVOSBundler implements IBundler {
    private static Logger logger = Logger.getLogger(TVOSBundler.class.getName());

    private void copyIcon(BobProjectProperties projectProperties, String projectRoot, File appDir, String name, String outName)
            throws IOException {
        String resource = projectProperties.getStringValue("ios", name);
        if (resource != null && resource.length() > 0) {
            File inFile = new File(projectRoot, resource);
            File outFile = new File(appDir, outName);
            FileUtils.copyFile(inFile, outFile);
        }
    }

    @Override
    public void bundleApplication(Project project, File bundleDir)
            throws IOException, CompileExceptionError {

        BobProjectProperties projectProperties = project.getProjectProperties();
        String exeArm64TVOS = Bob.getDmengineExe(Platform.Arm64TvOS, project.hasOption("debug"));
        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        
        System.out.println("Using engine from " + exeArm64TVOS);

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
            for (String name : Arrays.asList("game.projectc", "game.darc")) {
                FileUtils.copyFile(new File(buildDir, name), new File(appDir, name));
            }
        } else {
            FileUtils.copyDirectory(buildDir, appDir);
            new File(buildDir, "game.darc").delete();
        }

        // Copy ResourceRules.plist
        InputStream resourceRulesIn = getClass().getResourceAsStream("resources/ios/ResourceRules.plist");
        File resourceRulesOutFile = new File(appDir, "ResourceRules.plist");
        FileUtils.copyInputStreamToFile(resourceRulesIn, resourceRulesOutFile);
        resourceRulesIn.close();

        // Copy icons
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_57x57", "Icon.png");
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_114x114", "Icon@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_72x72", "Icon-72.png");
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_144x144", "Icon-72@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_76x76", "Icon-76.png");
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_152x152", "Icon-76@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_120x120", "Icon-60@2x.png");
        copyIcon(projectProperties, projectRoot, appDir, "app_icon_180x180", "Icon-60@3x.png");

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

        String facebookAppId = projectProperties.getStringValue("facebook", "appid", null);
        List<String> urlSchemes = new ArrayList<String>();

        if (facebookAppId != null) {
            urlSchemes.add(facebookAppId);
        }

        Map<String, Object> properties = new HashMap<String, Object>();
        properties.put("url-schemes", urlSchemes);

        BundleHelper helper = new BundleHelper(project, Platform.Arm64TvOS, bundleDir, ".app");
        helper.format(properties, "tvos", "infoplist", "resources/tvos/Info.plist", new File(appDir, "Info.plist"));

        // Copy Provisioning Profile
        FileUtils.copyFile(new File(provisioningProfile), new File(appDir, "embedded.mobileprovision"));

        // Copy Executable
        FileUtils.copyFile(new File(exeArm64TVOS), new File(appDir, title));

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
                throw new RuntimeException(e);
            }


            ProcessBuilder processBuilder = new ProcessBuilder("codesign",
                    "-f", "-s", identity, "--resource-rules="
                            + resourceRulesOutFile.getAbsolutePath(),
                    "--entitlements", entitlementOut.getAbsolutePath(),
                    appDir.getAbsolutePath());
            processBuilder.environment().put("EMBEDDED_PROFILE_NAME",
                    "embedded.mobileprovision");
            processBuilder.environment().put("CODESIGN_ALLOCATE", Bob.getExe(Platform.getHostPlatform(), "codesign_allocate"));

            Process process = processBuilder.start();


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

        File zipFile = new File(bundleDir, title + ".ipa");
        ZipOutputStream zipStream = new ZipOutputStream(new FileOutputStream(zipFile));

        Collection<File> files = FileUtils.listFiles(appDir, null, true);
        String root = FilenameUtils.normalize(bundleDir.getPath(), true);
        for (File f : files) {
            String p = FilenameUtils.normalize(f.getPath(), true);
            String rel = p.substring(root.length());

            // NOTE: The path to Payload is relative, i.e. not /Payload
            // If rooted iTunes complains about invalid package
            zipStream.putNextEntry(new ZipEntry("Payload" + rel));

            FileInputStream input = new FileInputStream(f);
            IOUtils.copy(input, zipStream);
            input.close();
            zipStream.closeEntry();
        }

        zipStream.close();
    }

}
