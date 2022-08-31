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

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.StringWriter;
import java.net.URL;
import java.net.ConnectException;
import java.nio.file.FileSystem;
import java.nio.file.FileSystems;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.text.ParseException;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.HashSet;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.io.filefilter.DirectoryFileFilter;
import org.apache.commons.io.filefilter.RegexFileFilter;
import org.apache.http.NoHttpResponseException;

import com.defold.extender.client.ExtenderClient;
import com.defold.extender.client.ExtenderClientException;
import com.defold.extender.client.ExtenderResource;
import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.MultipleCompileException;
import com.dynamo.bob.MultipleCompileException.Info;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.pipeline.ExtenderUtil.FileExtenderResource;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;
import com.samskivert.mustache.Mustache;
import com.samskivert.mustache.Template;

import java.awt.AlphaComposite;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.image.BufferedImage;
import javax.imageio.ImageIO;


public class BundleHelper {
    private Project project;
    private Platform platform;
    private BobProjectProperties projectProperties;
    private String title;
    private File buildDir;
    private File appDir;
    private String variant;
    private Map<String, Map<String, Object>> propertiesMap;

    public static final String MANIFEST_NAME_ANDROID    = "AndroidManifest.xml";
    public static final String MANIFEST_NAME_IOS        = "Info.plist";
    public static final String MANIFEST_NAME_MACOS      = "Info.plist";
    public static final String MANIFEST_NAME_HTML5      = "engine_template.html";

    public static final String SSL_CERTIFICATES_NAME   = "ssl_keys.pem";
    private static final String[] ARCHIVE_FILE_NAMES = {
        "game.projectc",
        "game.arci",
        "game.arcd",
        "game.dmanifest",
        "game.public.der"
    };

    private static Logger logger = Logger.getLogger(BundleHelper.class.getName());

    public static void throwIfCanceled(ICanceled canceled) {
        if(canceled.isCanceled()) {
            throw new RuntimeException("Canceled");
        }
    }

    public BundleHelper(Project project, Platform platform, File bundleDir, String variant) throws CompileExceptionError {
        this.projectProperties = project.getProjectProperties();
        this.propertiesMap = this.projectProperties.createTypedMap(new BobProjectProperties.PropertyType[]{BobProjectProperties.PropertyType.BOOL});

        this.project = project;
        this.platform = platform;
        this.title = this.projectProperties.getStringValue("project", "title", "Unnamed");

        String appDirSuffix = "";
        if (platform == Platform.X86_64MacOS || platform == Platform.Arm64Ios || platform == Platform.X86_64Ios) {
            appDirSuffix = ".app";
        }

        this.buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());
        this.appDir = new File(bundleDir, title + appDirSuffix);

        this.variant = variant;
    }

    public static String[] getArchiveFilenames(File buildDir) {
        File sslCerts = new File(buildDir, SSL_CERTIFICATES_NAME);
        if (sslCerts.exists()) {
            String[] files = Arrays.copyOf(ARCHIVE_FILE_NAMES, ARCHIVE_FILE_NAMES.length + 1);
            files[ARCHIVE_FILE_NAMES.length] = SSL_CERTIFICATES_NAME;
            return files;
        }
        return ARCHIVE_FILE_NAMES;
    }

    public static String projectNameToBinaryName(String projectName) {
        String output = projectName.replaceAll("[^a-zA-Z0-9_]", "");
        if (output.equals("")) {
            return "dmengine";
        }
        return output;
    }

    private IResource getResource(String category, String key) throws IOException {
        String val = this.projectProperties.getStringValue(category, key);
        if (val != null && val.trim().length() > 0) {
            return project.getResource(val);
        }
        throw new IOException(String.format("No resource found for %s.%s", category, key));
    }

    private String formatResource(Map<String, Object> properties, IResource resource) throws IOException {
        byte[] data = resource.getContent();
        if (data == null) {
            return "";
        }
        String s = new String(data);
        Template template = Mustache.compiler().compile(s);
        StringWriter sw = new StringWriter();
        template.execute(this.propertiesMap, properties, sw);
        sw.flush();
        return sw.toString();
    }

    private void formatResourceToFile(Map<String, Object> properties, IResource resource, File toFile) throws IOException {
        FileUtils.write(toFile, formatResource(properties, resource));
    }

    private String getManifestName(Platform platform) {
        if (platform == Platform.Arm64Ios || platform == Platform.X86_64Ios) {
            return BundleHelper.MANIFEST_NAME_IOS;
        } else if (platform == Platform.X86_64MacOS) {
            return BundleHelper.MANIFEST_NAME_MACOS;
        } else if (platform == Platform.Armv7Android || platform == Platform.Arm64Android) {
            return BundleHelper.MANIFEST_NAME_ANDROID;
        } else if (platform == Platform.JsWeb || platform == Platform.WasmWeb) {
            return BundleHelper.MANIFEST_NAME_HTML5;
        }
        return null;
    }

    public File getTargetManifestDir(Platform platform){
        String outputDir = project.getBinaryOutputDirectory();
        File buildDir = new File(FilenameUtils.concat(outputDir, platform.getExtenderPair()));
        File manifestsDir = new File(buildDir, "manifests");
        return manifestsDir;
    }

    // This is used in the step prior to upload all sources (and manifests) to the extender server
    // Each manifest has to be named like the default name (much easier for the server), even the main manifest file
    // This isn't an issue since there cannot be two manifests in the same folder
    public List<ExtenderResource> writeManifestFiles(Platform platform, File manifestDir) throws CompileExceptionError, IOException {
        List<ExtenderResource> resolvedManifests = new ArrayList<>();

        String title = projectProperties.getStringValue("project", "title", "Unnamed");
        String exeName = BundleHelper.projectNameToBinaryName(title);

        IResource mainManifest;
        Map<String, Object> properties = new HashMap<>();
        if (platform == Platform.Arm64Ios || platform == Platform.X86_64Ios) {
            mainManifest = getResource("ios", "infoplist");
            properties = createIOSManifestProperties(exeName);
        } else if (platform == Platform.X86_64MacOS) {
            mainManifest = getResource("osx", "infoplist");
            properties = createOSXManifestProperties(exeName);
        } else if (platform == Platform.Armv7Android || platform == Platform.Arm64Android) {
            mainManifest = getResource("android", "manifest");
            properties = createAndroidManifestProperties(exeName);
        } else if (platform == Platform.JsWeb || platform == Platform.WasmWeb) {
            mainManifest = getResource("html5", "htmlfile");
            properties = createHtml5ManifestProperties(exeName);
        } else {
            return resolvedManifests;
        }

        String mainManifestName = getMainManifestName(platform);

        // First, list all extension manifests
        List<IResource> sourceManifests = ExtenderUtil.getExtensionPlatformManifests(project, platform, getManifestName(platform));
        // Put the main manifest in front
        sourceManifests.add(0, mainManifest);

        // Resolve all properties in each manifest
        for (IResource resource : sourceManifests) {
            File manifest = new File(manifestDir, resource.getPath());
            File parent = manifest.getParentFile();
            if (!parent.exists()) {
                parent.mkdirs();
            }

            formatResourceToFile(properties, resource, manifest);

            String path = resource.getPath();
            // Store the main manifest at the root (and not in e.g. builtins/manifests/...)
            if (path.equals(mainManifest.getPath())) {
                path = mainManifestName;
            }
            resolvedManifests.add(new FileExtenderResource(manifest, path));
        }

        return resolvedManifests;
    }

    private File getAppManifestFile(Platform platform, File appDir) {
        if (platform == Platform.Arm64Ios || platform == Platform.X86_64Ios) {
            return new File(appDir, "Info.plist");
        } else if (platform == Platform.X86_64MacOS) {
            return new File(appDir, "Contents/Info.plist");
        } else if (platform == Platform.Armv7Android || platform == Platform.Arm64Android) {
            return new File(appDir, "AndroidManifest.xml");
        } else if (platform == Platform.JsWeb || platform == Platform.WasmWeb) {
            return new File(appDir, "index.html");
        }
        return null;
    }

    private static String getMainManifestName(Platform platform) {
        if (platform == Platform.Arm64Ios || platform == Platform.X86_64Ios) {
            return MANIFEST_NAME_IOS;
        } else if (platform == Platform.X86_64MacOS) {
            return MANIFEST_NAME_MACOS;
        } else if (platform == Platform.Armv7Android || platform == Platform.Arm64Android) {
            return MANIFEST_NAME_ANDROID;
        } else if (platform == Platform.JsWeb || platform == Platform.WasmWeb) {
            return MANIFEST_NAME_HTML5;
        }
        return null;
    }

    // either copies the merged manifest or writes a new resolved manifest from single source file
    public File copyOrWriteManifestFile(Platform platform, File appDir) throws IOException, CompileExceptionError {
        File targetManifest = getAppManifestFile(platform, appDir);

        boolean hasExtensions = ExtenderUtil.hasNativeExtensions(project);

        File manifestFile;
        if (!hasExtensions) {
            // Write a resolved manifest to this directory (should only be one)
            List<ExtenderResource> manifests = writeManifestFiles(platform, getTargetManifestDir(platform));

            ExtenderResource resource = manifests.get(0);
            if (resource instanceof FileExtenderResource) {
                manifestFile = ((FileExtenderResource)resource).getFile();
            } else {
                throw new IOException("Manifest file is of wrong type");
            }
        } else {
            // At this stage, in case of a cloud build, we've written the main manifest down in the default name.
            String mainManifestName = getMainManifestName(platform);

            File extenderPlatformDir = new File(project.getRootDirectory(), "build/"+platform.getExtenderPair());
            manifestFile = new File(extenderPlatformDir, mainManifestName); // the merged manifest
        }
        FileUtils.copyFile(manifestFile, targetManifest);
        return targetManifest;
    }

    public static void createAndroidResourceFolders(File dir) throws IOException {
        FileUtils.forceMkdir(new File(dir, "drawable"));
        FileUtils.forceMkdir(new File(dir, "drawable-ldpi"));
        FileUtils.forceMkdir(new File(dir, "drawable-mdpi"));
        FileUtils.forceMkdir(new File(dir, "drawable-hdpi"));
        FileUtils.forceMkdir(new File(dir, "drawable-xhdpi"));
        FileUtils.forceMkdir(new File(dir, "drawable-xxhdpi"));
        FileUtils.forceMkdir(new File(dir, "drawable-xxxhdpi"));
    }

    public List<ExtenderResource> writeExtensionResources(Platform platform) throws IOException, CompileExceptionError {
        List<ExtenderResource> resources = new ArrayList<>();

        if (platform.equals(Platform.Armv7Android) || platform.equals(Platform.Arm64Android)) {
            File packagesDir = new File(buildDir, "packages");
            packagesDir.mkdir();

            File resDir = new File(packagesDir, "com.defold.android/res");
            resDir.mkdirs();

            BundleHelper.createAndroidResourceFolders(resDir);
            copyAndroidIcons(resDir);
            copyAndroidPushIcons(resDir);

            Map<String, IResource> androidResources = ExtenderUtil.getAndroidResources(project);
            ExtenderUtil.storeResources(packagesDir, androidResources);

            resources.addAll(ExtenderUtil.listFilesRecursive(buildDir, packagesDir));
        }

        return resources;
    }

    // from extender: ExtenderUtil.java  (public for testing)
    public static List<String> excludeItems(List<String> input, List<String> expressions) {
        List<String> items = new ArrayList<>();

        List<Pattern> patterns = new ArrayList<>();
        for (String expression : expressions) {
            patterns.add(Pattern.compile(expression));
        }
        for (String item : input) {
            boolean excluded = false;
            if (expressions.contains(item) ) {
                excluded = true;
            }
            else {
                for (Pattern pattern : patterns) {
                    Matcher m = pattern.matcher(item);
                    if (m.matches()) {
                        excluded = true;
                        break;
                    }
                }
            }
            if (!excluded) {
                items.add(item);
            }
        }
        return items;
    }

    private BufferedImage resizeImage(BufferedImage originalImage, int size) {
        BufferedImage resizedImage = new BufferedImage(size, size, BufferedImage.TYPE_INT_ARGB);
        Graphics2D g = resizedImage.createGraphics();

        g.setRenderingHint(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BILINEAR);
        g.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_QUALITY);
        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);

        g.drawImage(originalImage, 0, 0, size, size, null);
        g.dispose();
        g.setComposite(AlphaComposite.Src);

        return resizedImage;
    }

    private void genIcon(BufferedImage fallbackImage, File outputDir, String propertyCategory, String propertyName, String outName, int size) throws IOException
    {
        File outFile = new File(outputDir, outName);

        // If the property was found just copy icon file to output folder.
        if (propertyName.length() > 0) {
            String resource = projectProperties.getStringValue(propertyCategory, propertyName);
            if (resource != null && resource.length() > 0) {
                IResource inResource = project.getResource(resource);
                if (!inResource.exists()) {
                    throw new IOException(String.format("%s does not exist.", resource));
                }
                FileUtils.writeByteArrayToFile(outFile, inResource.getContent());
                return;
            }
        }

        // Resize fallback image if resource wasn't specified or found.
        ImageIO.write(resizeImage(fallbackImage, size), "png", outFile);
    }

    private BufferedImage getFallbackIconImage(String categoryName, String[] alternativeKeys) throws IOException
    {
        // Try to use the largest icon as a fallback, otherwise use a builtin icon.
        String largestIcon = null;
        for (String propName : alternativeKeys) {
            String resource = projectProperties.getStringValue(categoryName, propName);
            if (resource != null && resource.length() > 0) {
                largestIcon = resource;
            }
        }
        File largestIconFile = File.createTempFile("temp", "default_icon.png");

        if (largestIcon != null) {
            IResource largestIconRes = project.getResource(largestIcon);
            if (!largestIconRes.exists()) {
                throw new IOException("Could not find resource: " + largestIcon);
            }
            FileUtils.writeByteArrayToFile(largestIconFile, largestIconRes.getContent());
        } else {
            URL defaultIconURL = getClass().getResource("resources/ios/default_icon.png");
            FileUtils.writeByteArrayToFile(largestIconFile, IOUtils.toByteArray(defaultIconURL));
        }

        return ImageIO.read(largestIconFile);
    }

    public void copyIosIcons() throws IOException
    {
        // Get fallback icon as an image
        String[] iconPropNames = { "app_icon_76x76", "app_icon_120x120", "app_icon_152x152", "app_icon_167x167", "app_icon_180x180" };
        BufferedImage largestIconImage = getFallbackIconImage("ios", iconPropNames);

        // Copy game.project specified icons
        genIcon(largestIconImage, appDir, "ios", "app_icon_120x120",          "AppIcon60x60@2x.png", 120);
        genIcon(largestIconImage, appDir, "ios", "app_icon_180x180",          "AppIcon60x60@3x.png", 180);
        genIcon(largestIconImage, appDir, "ios",   "app_icon_76x76",        "AppIcon76x76~ipad.png",  76);
        genIcon(largestIconImage, appDir, "ios", "app_icon_152x152",     "AppIcon76x76@2x~ipad.png", 152);
        genIcon(largestIconImage, appDir, "ios", "app_icon_167x167", "AppIcon83.5x83.5@2x~ipad.png", 167);
    }

    public void copyAndroidIcons(File resDir) throws IOException
    {
        // Get fallback icon as an image
        String[] iconPropNames = { "app_icon_32x32", "app_icon_36x36", "app_icon_48x48", "app_icon_72x72", "app_icon_96x96", "app_icon_144x144", "app_icon_192x192" };
        BufferedImage largestIconImage = getFallbackIconImage("android", iconPropNames);

        // copy old 32x32 icon first, the correct size is actually 36x36
        genIcon(largestIconImage, resDir, "android",   "app_icon_32x32",    "drawable-ldpi/icon.png",  36);
        genIcon(largestIconImage, resDir, "android",   "app_icon_36x36",    "drawable-ldpi/icon.png",  36);
        genIcon(largestIconImage, resDir, "android",   "app_icon_48x48",    "drawable-mdpi/icon.png",  48);
        genIcon(largestIconImage, resDir, "android",   "app_icon_72x72",    "drawable-hdpi/icon.png",  72);
        genIcon(largestIconImage, resDir, "android",   "app_icon_96x96",   "drawable-xhdpi/icon.png",  96);
        genIcon(largestIconImage, resDir, "android", "app_icon_144x144",  "drawable-xxhdpi/icon.png", 144);
        genIcon(largestIconImage, resDir, "android", "app_icon_192x192", "drawable-xxxhdpi/icon.png", 192);
    }

    public void copyAndroidPushIcons(File resDir) throws IOException {
        copyFile(projectProperties, project.getRootDirectory(), resDir, "android", "push_icon_small", "drawable/push_icon_small.png");
        copyFile(projectProperties, project.getRootDirectory(), resDir, "android", "push_icon_large", "drawable/push_icon_large.png");

        String[] dpis = new String[] { "ldpi", "mdpi", "hdpi", "xhdpi", "xxhdpi", "xxxhdpi" };
        for (String dpi : dpis) {
            copyIconDPI(projectProperties, project.getRootDirectory(), resDir, "android", "push_icon_small", "push_icon_small.png", dpi);
            copyIconDPI(projectProperties, project.getRootDirectory(), resDir, "android", "push_icon_large", "push_icon_large.png", dpi);
        }
    }

    private boolean copyFile(BobProjectProperties projectProperties, String projectRoot, File resDir, String category, String name, String outName)
            throws IOException {
        String resource = projectProperties.getStringValue(category, name);
        if (resource != null && resource.length() > 0) {
            File inFile = new File(projectRoot, resource);
            File outFile = new File(resDir, outName);
            FileUtils.copyFile(inFile, outFile);
            return true;
        }
        return false;
    }

    private boolean copyIconDPI(BobProjectProperties projectProperties, String projectRoot, File resDir, String category, String name, String outName, String dpi)
            throws IOException {
            return copyFile(projectProperties, projectRoot, resDir, category, name + "_" + dpi, "drawable-" + dpi + "/" + outName);
    }

    public static List<String> createArrayFromString(String line) {
        if (line == null)
            return new ArrayList<String>();
        line = line.trim();
        if (line.isEmpty())
            return new ArrayList<String>();
        return new ArrayList<String>(Arrays.asList(line.split("\\s*,\\s*")));
    }

    public Map<String, Object> createAndroidManifestProperties(String exeName) throws IOException {
        Map<String, Object> properties = new HashMap<>();
        properties.put("exe-name", exeName);

        // We copy and resize the default icon in builtins if no other icons are set.
        // This means that the app will always have icons from now on.
        properties.put("has-icons?", true);

        if(projectProperties.getBooleanValue("display", "dynamic_orientation", false)==false) {
            Integer displayWidth = projectProperties.getIntValue("display", "width", 960);
            Integer displayHeight = projectProperties.getIntValue("display", "height", 640);
            if((displayWidth != null & displayHeight != null) && (displayWidth > displayHeight)) {
                properties.put("orientation-support", "landscape");
            } else {
                properties.put("orientation-support", "portrait");
            }
        } else {
            properties.put("orientation-support", "sensor");
        }

        // Since we started to always fill in the default values to the propject properties
        // it is harder to distinguish what is a user defined value.
        // For certain properties, we'll update them automatically in the build step (unless they already exist in game.project)
        if (projectProperties.isDefault("android", "debuggable")) {
            Map<String, Object> propGroup = this.propertiesMap.get("android");
            if (propGroup != null && propGroup.containsKey("debuggable")) {
                boolean debuggable = this.variant.equals(Bob.VARIANT_DEBUG);
                propGroup.put("debuggable", debuggable ? "true":"false");
            }
        }

        return properties;
    }

    private String derivedBundleName() {
        String title = projectProperties.getStringValue("project", "title", "dmengine");
        return title.substring(0, Math.min(title.length(), 15));
    }

    public Map<String, Object> createOSXManifestProperties(String exeName) throws IOException {
        Map<String, Object> properties = new HashMap<>();
        properties.put("exe-name", exeName);

        String applicationLocalizationsStr = projectProperties.getStringValue("osx", "localizations", null);
        List<String> applicationLocalizations = createArrayFromString(applicationLocalizationsStr);
        properties.put("application-localizations", applicationLocalizations);

        properties.put("bundle-name", projectProperties.getStringValue("osx", "bundle_name", derivedBundleName()));

        return properties;
    }

    public Map<String, Object> createIOSManifestProperties(String exeName) throws IOException {
        Map<String, Object> properties = new HashMap<>();

        List<String> applicationQueriesSchemes = new ArrayList<String>();
        List<String> urlSchemes = new ArrayList<String>();
        String bundleId = projectProperties.getStringValue("ios", "bundle_identifier");
        if (bundleId != null) {
            urlSchemes.add(bundleId);
        }

        properties.put("exe-name", exeName);
        properties.put("url-schemes", urlSchemes);
        properties.put("application-queries-schemes", applicationQueriesSchemes);
        properties.put("bundle-name", projectProperties.getStringValue("ios", "bundle_name", derivedBundleName()));
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

        String applicationLocalizationsStr = projectProperties.getStringValue("ios", "localizations", null);
        List<String> applicationLocalizations = createArrayFromString(applicationLocalizationsStr);
        properties.put("application-localizations", applicationLocalizations);

        return properties;
    }


    public Map<String, Object> createHtml5ManifestProperties(String exeName) throws IOException {
        Map<String, Object> properties = new HashMap<>();

        properties.put("exe-name", exeName);

        // Same value as engine is compiled with; 268435456
        int customHeapSize = projectProperties.getIntValue("html5", "heap_size", 256) * 1024 * 1024;

        {// Deprecated method of setting the heap size. For backwards compatibility
            if (projectProperties.getBooleanValue("html5", "set_custom_heap_size", false)) {
                Integer size = projectProperties.getIntValue("html5", "custom_heap_size");
                if (null != size) {
                    customHeapSize = size.intValue();
                }
            }
        }
        properties.put("DEFOLD_HEAP_SIZE", customHeapSize);

        String splashImage = projectProperties.getStringValue("html5", "splash_image", null);
        if (splashImage != null) {
            properties.put("DEFOLD_SPLASH_IMAGE", new File(project.getRootDirectory(), splashImage).getName());
        } else {
            // Without this value we can't use Inverted Sections (^) in Mustache and recive an error:
            // "No key, method or field with name 'DEFOLD_SPLASH_IMAGE' on line N"
            properties.put("DEFOLD_SPLASH_IMAGE", false);
        }

        // Check if game has configured a Facebook App ID
        String facebookAppId = projectProperties.getStringValue("facebook", "appid", null);
        properties.put("DEFOLD_HAS_FACEBOOK_APP_ID", facebookAppId != null ? "true" : "false");

        String engineArgumentsString = projectProperties.getStringValue("html5", "engine_arguments", null);
        List<String> engineArguments = createArrayFromString(engineArgumentsString);

        properties.put("DEFOLD_ARCHIVE_LOCATION_PREFIX", projectProperties.getStringValue("html5", "archive_location_prefix", "archive"));
        properties.put("DEFOLD_ARCHIVE_LOCATION_SUFFIX", projectProperties.getStringValue("html5", "archive_location_suffix", ""));
        properties.put("DEFOLD_ENGINE_ARGUMENTS", engineArguments);

        String scaleMode = projectProperties.getStringValue("html5", "scale_mode", "downscale_fit").toUpperCase();
        properties.put("DEFOLD_SCALE_MODE_IS_"+scaleMode, true);

        /// Legacy properties for backwards compatibility
        {
            properties.put("DEFOLD_DISPLAY_WIDTH", projectProperties.getIntValue("display", "width"));
            properties.put("DEFOLD_DISPLAY_HEIGHT", projectProperties.getIntValue("display", "height"));

            String version = projectProperties.getStringValue("project", "version", "0.0");
            properties.put("DEFOLD_APP_TITLE", String.format("%s %s", title, version));

            properties.put("DEFOLD_BINARY_PREFIX", exeName);
        }

        // When running "Build HTML and Launch" we need to ignore the archive location prefix/suffix.
        Boolean localLaunch = project.option("local-launch", "false").equals("true");
        if (localLaunch) {
            properties.put("DEFOLD_ARCHIVE_LOCATION_PREFIX", "archive");
            properties.put("DEFOLD_ARCHIVE_LOCATION_SUFFIX", "");
            properties.put("HAS_DEFOLD_ENGINE_ARGUMENTS", "true");

            engineArguments.add("--verify-graphics-calls=false");
            properties.put("DEFOLD_ENGINE_ARGUMENTS", engineArguments);
        }

        IResource customCSS = getResource("html5", "cssfile");
        properties.put("DEFOLD_CUSTOM_CSS_INLINE", formatResource(properties, customCSS));

        return properties;
    }

    public static class ResourceInfo
    {
        public String severity;
        public String resource;
        public String message;
        public int lineNumber;

        public ResourceInfo(String severity, String resource, String lineNumber, String message) {
            this.severity = severity == null ? "error" : severity;
            this.resource = resource;
            this.message = message;
            this.lineNumber = Integer.parseInt(lineNumber.equals("") ? "1" : lineNumber);
        }
    };

    // These regexp's works for both cpp and javac errors, warnings and note entries associated with a resource.
    private static Pattern resourceIssueGCCRe = Pattern.compile("^(?:(?:(?:\\/tmp\\/job[0-9]*\\/)?(?:upload\\/packages|upload|build)\\/)|(?:.*\\/drive_c\\/))?([^:]+):([0-9]+):([0-9]*)?:?\\s*(error|warning|note|):?\\s*(.+)"); // GCC + Clang + Java
    private static Pattern resourceIssueCLRe = Pattern.compile("^(?:upload\\/packages|upload|build)\\/([^:]+)\\(([0-9]+)\\)([0-9]*):\\s*(fatal error|error|warning|note).*?:\\s*(.+)"); // CL.exe
    private static Pattern resourceIssueLinkerLINKRe = Pattern.compile("^.+?\\.lib\\((.+?)\\)\\s:([0-9]*)([0-9]*)\\s*(error|warning|note).*?:\\s*(.+)"); // LINK.exe (the line/column numbers won't really match anything)
    private static Pattern resourceIssueLinkerCLANGRe = Pattern.compile("^(Undefined symbols for architecture [\\w]+:\\n.*?referenced from:\\n.*)");
    private static Pattern resourceIssueLinkerLLDLINKre = Pattern.compile("^(?:.*lld-link|.*ld):\\s(?:(warning|error)?:\\s)?(?:([\\w-.]+)\\([\\w.]+\\):\\s)?(.*)");

    // Some errors/warning have an extra line before or after the the reported error, which is also very good to have
    private static Pattern resourceIssueLineBeforeRe = Pattern.compile("^.*upload\\/([^:]+):\\s*(.+)");

    private static Pattern resourceIssueLinkerLINKLibError = Pattern.compile("^(.+\\.lib)(\\(.+\\.obj\\)\\s:\\s.+)");
    private static Pattern resourceIssueLinkerLINKCatchAll = Pattern.compile("^(.+error\\s.+)");

    // Matches ext.manifest and also _app/app.manifest
    private static Pattern manifestIssueRe = Pattern.compile("^.+'(.+\\.manifest)'.+");

    // This regexp catches errors, warnings and note entries that are not associated with a resource.
    private static Pattern nonResourceIssueRe = Pattern.compile("^(fatal|error|warning|note):\\s*(.+)");

    // This regexp catches a malformed SDK where content folders are missing
    private static Pattern missingSDKFolderLinkerCLANGRe = Pattern.compile("^ld: warning: directory not found for option '-L.*\\/([0-9a-f]{40})\\/defoldsdk\\/.*'\n[\\s\\S]*");

    // This regexp catches linker errors where specified libraries are missing
    private static Pattern missingLibraryLinkerCLANGRe = Pattern.compile("^ld: library not found for -l(.+)\n[\\s\\S]*");

    // This regexp catches conflicts between jar files
    private static Pattern jarConflictIssue = Pattern.compile("Uncaught translation error:*.+");

    // In case something really bad happened on the server
    private static Pattern internalServerIssue = Pattern.compile("Internal Server Error*.+");

    // This regexp catches issues when merging android manifests
    private static Pattern manifestMergeAndroidRe = Pattern.compile("(?:\\/tmp\\/job\\d*?\\/)(?:build|upload)\\/(.*?):(\\d*?):\\s(.*?):\\s(.*)");

    private static List<String> excludeMessages = new ArrayList<String>() {{
        add("[options] bootstrap class path not set in conjunction with -source 1.6"); // Mighty annoying message
    }};

    private static void parseLogGCC(String[] lines, List<ResourceInfo> issues) {
        final Pattern compilerPattern = resourceIssueGCCRe; // catches linker errors too

        for (int count = 0; count < lines.length; ++count) {
            String line = lines[count];
            Matcher m = compilerPattern.matcher(line);

            if (m.matches()) {
                // Groups: resource, line, column, "error", message
                String severity = m.group(4);
                if (severity == null || severity.equals(""))
                    severity = "error";
                BundleHelper.ResourceInfo info = new BundleHelper.ResourceInfo(severity, m.group(1), m.group(2), m.group(5));
                issues.add(info);

                // Some errors have a preceding line (with the same file name, but no line number)
                if (count > 0) {
                    String lineBefore = lines[count-1];
                    m = compilerPattern.matcher(lineBefore);
                    if (!m.matches() && lineBefore.contains(info.resource)) {
                        m = BundleHelper.resourceIssueLineBeforeRe.matcher(lineBefore);
                        if (m.matches()) {
                            if (info.resource.equals(m.group(1)))
                                info.message = m.group(2) + "\n" + info.message;
                        }
                    }
                }

                // Also, there might be a trailing line that belongs (See BundleHelperTest for examples)
                if (count+1 < lines.length) {
                    String lineAfter = lines[count+1];
                    m = BundleHelper.resourceIssueLineBeforeRe.matcher(lineAfter);
                    if (!line.equals("") && !m.matches()) {
                        info.message = info.message + "\n" + lineAfter;
                        count++;
                    }
                }
                continue;
            }

            m = resourceIssueLinkerLLDLINKre.matcher(line);
            if (m.matches()) {
                // Groups: severity, resource, message
                issues.add(new BundleHelper.ResourceInfo(m.group(1), m.group(2), "", m.group(3)));
                continue;
            }
        }
    }

    private static void parseLogClang(String[] lines, List<ResourceInfo> issues) {
        final Pattern linkerPattern = resourceIssueLinkerCLANGRe;
        final Pattern linkerMissingSDKFolderPattern = missingSDKFolderLinkerCLANGRe;
        final Pattern linkerMissingLibraryLinkerCLANGRe = missingLibraryLinkerCLANGRe;

        // Very similar, with lookBehind and lookAhead for errors
        parseLogGCC(lines, issues);

        for (int count = 0; count < lines.length; ++count) {
            String line = lines[count];
            // Compare with some lookahead if it matches
            for (int i = 1; i <= 2 && (count+i) < lines.length; ++i) {
                line += "\n" + lines[count+i];
            }
            Matcher m = linkerPattern.matcher(line);
            if (m.matches()) {
                // Groups: message
                issues.add(new BundleHelper.ResourceInfo("error", null, "", m.group(1)));
            }
            m = linkerMissingSDKFolderPattern.matcher(line);
            if (m.matches()) {
                issues.add(new BundleHelper.ResourceInfo("error", null, "", "Invalid Defold SDK: '" + m.group(1) + "'"));
            }
            m = linkerMissingLibraryLinkerCLANGRe.matcher(line);
            if (m.matches()) {
                issues.add(new BundleHelper.ResourceInfo("error", null, "", "Missing library '" + m.group(1) + "'"));
            }
        }
    }

    private static void parseLogWin32(String[] lines, List<ResourceInfo> issues) {

        for (int count = 0; count < lines.length; ++count) {
            String line = lines[count];
            Matcher m = resourceIssueCLRe.matcher(line);
            if (m.matches()) {
                // Groups: resource, line, column, "error", message
                issues.add(new BundleHelper.ResourceInfo(m.group(4), m.group(1), m.group(2), m.group(5)));
            }

            m = resourceIssueLinkerLINKRe.matcher(line);
            if (m.matches()) {
                // Groups: resource, line, column, "error", message
                issues.add(new BundleHelper.ResourceInfo(m.group(4), m.group(1), m.group(2), m.group(5)));
            }

            m = resourceIssueLinkerLINKLibError.matcher(line);
            if (m.matches()) {
                // Groups: resource, message
                issues.add(new BundleHelper.ResourceInfo("error", m.group(1), "", line));
            }

            m = resourceIssueLinkerLINKCatchAll.matcher(line);
            if (m.matches()) {
                // Groups: message
                issues.add(new BundleHelper.ResourceInfo("error", null, "", line));
            }
        }

        parseLogClang(lines, issues);

        for (int count = 0; count < lines.length; ++count) {
            String line = lines[count];
            Matcher m = resourceIssueLinkerLLDLINKre.matcher(line);
            if (m.matches()) {
                // Groups: severity, resource, message
                issues.add(new BundleHelper.ResourceInfo(m.group(1), m.group(2), "", m.group(3)));
            }
        }
    }

    private static void parseJarConflicts(String[] lines, List<ResourceInfo> issues) {
        final Pattern compilerPattern = jarConflictIssue;

        for (int count = 0; count < lines.length; ++count) {
            String line = lines[count];
            Matcher m = compilerPattern.matcher(line);

            if (m.matches()) {
                BundleHelper.ResourceInfo info = new BundleHelper.ResourceInfo("error", null, "", m.group(0));
                issues.add(info);
            }
        }
    }

    public static void parseLog(String platform, String log, List<ResourceInfo> issues) throws CompileExceptionError {
        String[] lines = log.split("\\r?\\n");

        List<ResourceInfo> allIssues = new ArrayList<ResourceInfo>();
        if (platform.contains("osx") || platform.contains("ios") || platform.contains("web")) {
            parseLogClang(lines, allIssues);
        } else if (platform.contains("android")) {
            parseJarConflicts(lines, allIssues);
            parseLogGCC(lines, allIssues);
        } else if (platform.contains("linux")) {
            parseLogGCC(lines, allIssues);
        } else if (platform.contains("win32")) {
            parseLogWin32(lines, allIssues);
        }

        for (int count = 0; count < lines.length; ++count) {
            String line = lines[count];

            // A bit special. Currently, no more errors are delivered after this
            // So we can consume the rest of the log into this message
            Matcher m = BundleHelper.manifestIssueRe.matcher(line);
            if (m.matches()) {
                allIssues.add( new BundleHelper.ResourceInfo("error", m.group(1), "1", log) );
                return;
            }

            m = BundleHelper.internalServerIssue.matcher(line);
            if (m.matches()) {
                throw new CompileExceptionError(null, 0, "Internal Server Error. Read the full logs on disc");
            }

            m = BundleHelper.nonResourceIssueRe.matcher(line);

            if (m.matches()) {
                allIssues.add( new BundleHelper.ResourceInfo(m.group(1), null, "0", m.group(2)) );
                continue;
            }
        }

        for (ResourceInfo info : allIssues) {
            if (excludeMessages.contains(info.message)) {
                continue;
            }
            issues.add(info);
        }
    }

    private static void checkForDuplicates(List<ExtenderResource> resources) throws CompileExceptionError {
        Set<String> uniquePaths = new HashSet<>();
        for (ExtenderResource resource : resources) {
            String path = resource.getPath(); // The relative path
            if (uniquePaths.contains(path)) {
                IResource iresource = ExtenderUtil.getResource(path, resources);
                throw new CompileExceptionError(iresource, -1, "Duplicate file in upload zip: " + resource.getAbsPath());
            }
            uniquePaths.add(path);
        }
    }

    public static File buildEngineRemote(Project project, ExtenderClient extender, String platform, String sdkVersion, List<ExtenderResource> allSource, File logFile, boolean async) throws ConnectException, NoHttpResponseException, CompileExceptionError, MultipleCompileException {
        File zipFile = null;

        try {
            zipFile = File.createTempFile("build_" + sdkVersion, ".zip");
            zipFile.deleteOnExit();
        } catch (IOException e) {
            throw new CompileExceptionError("Failed to create temp zip file", e.getCause());
        }

        checkForDuplicates(allSource);

        try {
            extender.build(platform, sdkVersion, allSource, zipFile, logFile, async);
        } catch (ExtenderClientException e) {
            if (e.getCause() instanceof ConnectException) {
                throw (ConnectException)e.getCause();
            }
            if (e.getCause() instanceof NoHttpResponseException) {
                throw (NoHttpResponseException)e.getCause();
            }

            String buildError = "<no log file>";
            if (logFile != null) {
                try {
                    List<ResourceInfo> issues = new ArrayList<>();
                    buildError = FileUtils.readFileToString(logFile);
                    parseLog(platform, buildError, issues);
                    MultipleCompileException exception = new MultipleCompileException("Build error", e);
                    IResource projectResource = project.getGameProjectResource();
                    IResource extManifestResource = ExtenderUtil.getResource(allSource.get(0).getPath(), allSource);
                    if (extManifestResource == null) {
                        extManifestResource = projectResource;
                    }

                    IResource contextResource = null;

                    for (ResourceInfo info : issues) {

                        IResource issueResource = info.resource == null ? null : ExtenderUtil.getResource(info.resource, allSource);

                        // If it's the app manifest, let's translate it back into its original name
                        if (info.resource != null && info.resource.endsWith(ExtenderClient.appManifestFilename)) {
                            for (ExtenderResource extResource : allSource) {
                                if (extResource.getAbsPath().endsWith(info.resource)) {
                                    issueResource = ((ExtenderUtil.FSAppManifestResource)extResource).getResource();
                                    info.message = info.message.replace(extResource.getPath(), issueResource.getPath());
                                    break;
                                }
                            }
                        }

                        IResource exceptionResource = issueResource == null ? extManifestResource : issueResource;
                        int severity = info.severity.contains("error") ? Info.SEVERITY_ERROR : info.severity.equals("warning") ? Info.SEVERITY_WARNING : Info.SEVERITY_INFO;
                        exception.addIssue(severity, exceptionResource, info.message, info.lineNumber);

                        String msg = String.format("%s(%d): %s", info.resource != null ? info.resource : "<unknown>", info.lineNumber, info.message);
                        logger.log(severity == Info.SEVERITY_ERROR ? Level.SEVERE : Level.WARNING, msg);

                        // The first resource generating errors should be related - we can use it to give context to the raw log.
                        if (contextResource == null && issueResource != null) {
                            contextResource = issueResource;
                        }
                    }

                    // If we do not yet have a context resource - fall back on the project resource
                    if (contextResource == null) {
                        contextResource = projectResource;
                    }

                    exception.setLogPath(logFile.getAbsolutePath());

                    exception.attachLog(contextResource, buildError);
                    throw exception;
                } catch (IOException ioe) {
                    buildError = "<failed reading log>";
                } catch (CompileExceptionError ioe) {
                    buildError = String.format("'%s' could not be built. Sdk version: '%s'\nLog: '%s'", platform, sdkVersion, buildError);
                    throw new CompileExceptionError(buildError, e.getCause());
                }
            }
            buildError = String.format("'%s' could not be built. Sdk version: '%s'\nLog: '%s'", platform, sdkVersion, buildError);
            throw new CompileExceptionError(buildError, e.getCause());
        }

        return zipFile;
    }

    // From extender's ZipUtils
    public static void unzip(InputStream inputStream, Path targetDirectory) throws IOException {
        try (ZipInputStream zipInputStream = new ZipInputStream(inputStream)) {
            ZipEntry zipEntry = zipInputStream.getNextEntry();

            while (zipEntry != null) {
                File entryTargetFile = new File(targetDirectory.toFile(), zipEntry.getName());

                if (zipEntry.isDirectory()) {
                    Files.createDirectories(entryTargetFile.toPath());
                } else {
                    File parentDir = entryTargetFile.getParentFile();
                    if (!parentDir.exists()) {
                        Files.createDirectories(parentDir.toPath());
                    }
                    extractFile(zipInputStream, entryTargetFile);
                }

                zipInputStream.closeEntry();
                zipEntry = zipInputStream.getNextEntry();
            }
        }
    }
    private static void extractFile(ZipInputStream zipIn, File file) throws IOException {
        BufferedOutputStream bos = new BufferedOutputStream(new FileOutputStream(file));
        byte[] bytesIn = new byte[128 * 1024];
        int read = 0;
        while ((read = zipIn.read(bytesIn)) != -1) {
            bos.write(bytesIn, 0, read);
        }
        bos.close();
    }

    public static List<IResource> listFilesRecursive(Project project, String path) {
        List<IResource> resources = new ArrayList<>();
        ArrayList<String> paths = new ArrayList<>();
        project.findResourcePaths(path, paths);
        for (String p : paths) {
            IResource r = project.getResource(p);

            // Note: findResourcePaths will return the supplied path even if it's not a file.
            // We need to check if the resource is not a directory before adding it to the list of paths found.
            if (r.isFile()) {
                resources.add(r);
            }
        }
        return resources;
    }

    public static List<File> getPipelinePlugins(Project project, String pluginsDir) {
        List<File> files = ExtenderUtil.listFilesRecursive(new File(pluginsDir), ExtenderUtil.JAR_RE);
        return files;
    }

    public static void extractPipelinePlugins(Project project, String pluginsDir) throws CompileExceptionError {
        List<IResource> sources = new ArrayList<>();
        List<String> extensionFolders = ExtenderUtil.getExtensionFolders(project);
        for (String extension : extensionFolders) {
            IResource resource = project.getResource(extension + "/" + ExtenderClient.extensionFilename);
            if (!resource.exists()) {
                throw new CompileExceptionError(resource, 1, "Resource doesn't exist!");
            }

            sources.addAll( listFilesRecursive( project, extension + "/plugins/" ) );
        }

        ExtenderUtil.storeResources(new File(pluginsDir), sources);
    }

}
