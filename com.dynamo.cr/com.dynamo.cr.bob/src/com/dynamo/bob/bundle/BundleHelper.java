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

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.StringWriter;
import java.net.URL;
import java.net.ConnectException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.HashSet;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import com.sun.istack.Nullable;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.lang3.StringUtils;
import org.apache.http.NoHttpResponseException;

import com.defold.extender.client.ExtenderClient;
import com.defold.extender.client.ExtenderClientException;
import com.defold.extender.client.ExtenderResource;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.MultipleCompileException;
import com.dynamo.bob.MultipleCompileException.Info;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.pipeline.ExtenderUtil.FileExtenderResource;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.FileUtil;
import com.samskivert.mustache.Mustache;
import com.samskivert.mustache.MustacheException;
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
    private IBundler platformBundler;
    private String title;
    private File buildDir;
    private File appDir;
    private String variant;
    private Map<String, Map<String, Object>> propertiesMap;

    private Map<String, Object> templateProperties = new HashMap<>();

    public static final String SSL_CERTIFICATES_NAME   = "ssl_keys.pem";
    private static final String[] ARCHIVE_FILE_NAMES = {
        "game.projectc",
        "game.arci",
        "game.arcd",
        "game.dmanifest",
        "game.public.der"
    };

    public static void throwIfCanceled(ICanceled canceled) {
        if(canceled.isCanceled()) {
            throw new RuntimeException("Canceled");
        }
    }

    private IBundler getOrCreateBundler() throws CompileExceptionError {
        if (this.platformBundler == null) {
            this.platformBundler = this.project.createBundler(this.platform);
        }
        return this.platformBundler;
    }

    public BundleHelper(Project project, Platform platform, File bundleDir, String variant, @Nullable IBundler bundler) throws CompileExceptionError {
        this.projectProperties = project.getProjectProperties();
        this.propertiesMap = this.projectProperties.createTypedMap(new BobProjectProperties.PropertyType[]{BobProjectProperties.PropertyType.BOOL});
        this.platformBundler = bundler;

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
        String projectNameNoAccents = StringUtils.stripAccents(projectName);
        String output = projectNameNoAccents.replaceAll("[^a-zA-Z0-9_]", "");
        if (output.equals("")) {
            return "dmengine";
        }
        return output;
    }

    public IResource getResource(String category, String key) throws IOException {
        return this.project.getResource(category, key);
    }

    public IResource getResource(String category, String key, boolean mustExist) throws IOException {
        IResource resource = this.getResource(category, key);
        if (mustExist && !resource.exists())
        {
            throw new IOException(String.format("Resource does not exist: '%s'  (%s.%s)", resource.getAbsPath(), category, key));
        }
        return resource;
    }

    static public String formatResource(Map<String, Map<String, Object>> propertiesMap, Map<String, Object> properties, IResource resource) throws IOException {
        return formatResource(propertiesMap, properties, resource.getContent(), resource.getPath());
    }
    static public String formatResource(Map<String, Map<String, Object>> propertiesMap, Map<String, Object> properties, byte[] data, final String sourceLocation) throws IOException {
        if (data == null) {
            return "";
        }
        String s = new String(data);
        Template template = Mustache.compiler().emptyStringIsFalse(true).compile(s);
        StringWriter sw = new StringWriter();
        try {
            template.execute(propertiesMap, properties, sw);
         } catch (MustacheException e) {
            MustacheException.Context context = (MustacheException.Context) e;
            String key = context.key;
            int lineNo = context.lineNo;
            String cause = String.format("File '%s' requires '%s' in line %d. Make sure you have '%s' in your game.project", sourceLocation, key, lineNo, key);
            throw new MustacheException(cause);
         }
        sw.flush();
        return sw.toString();
    }

    private String formatResource(byte[] content, final String sourceLocation) throws IOException {
        return formatResource(this.propertiesMap, this.templateProperties, content, sourceLocation);
    }

    private void formatResourceToFile(IResource resource, File toFile) throws IOException {
        formatResourceToFile(resource.getContent(), resource.getPath(),toFile);
    }

    public void formatResourceToFile(byte[] content, final String sourceLocation, File toFile) throws IOException {
        FileUtils.write(toFile, formatResource(content, sourceLocation));
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
        updateTemplateProperties();
        List<ExtenderResource> resolvedManifests = new ArrayList<>();
        IResource mainManifest;
        String mainManifestName;

        IBundler bundler = getOrCreateBundler();

        // The new code path we wish to use
        mainManifest = bundler.getManifestResource(project, platform);
        if (mainManifest == null) {
            return resolvedManifests;
        }

        mainManifestName = bundler.getMainManifestName(platform);

        // First, list all extension manifests
        List<IResource> sourceManifests = ExtenderUtil.getExtensionPlatformManifests(project, platform);
        // Put the main manifest in front
        sourceManifests.add(0, mainManifest);

        // Resolve all properties in each manifest
        for (IResource resource : sourceManifests) {
            File manifest = new File(manifestDir, resource.getPath());
            File parent = manifest.getParentFile();
            if (!parent.exists()) {
                parent.mkdirs();
            }

            formatResourceToFile(resource, manifest);

            String path = resource.getPath();
            // Store the main manifest at the root (and not in e.g. builtins/manifests/...)
            if (path.equals(mainManifest.getPath())) {
                path = mainManifestName;
            }
            resolvedManifests.add(new FileExtenderResource(manifest, path));
        }

        return resolvedManifests;
    }

    /**
     * Copy a PrivacyInfo.xcprivacy to a target folder. The file will either be
     * copied from the extender build results or if that doesn't exist it will
     * copy from the privacy manifest set in game.project
     * @param project
     * @param platform
     * @param appDir Directory to copy to
     */
    public static void copyPrivacyManifest(Project project, Platform platform, File appDir) throws IOException {
        final String privacyManifestFilename = "PrivacyInfo.xcprivacy";
        File targetPrivacyManifest = new File(appDir, privacyManifestFilename);

        File extenderBuildDir = new File(project.getRootDirectory(), "build");
        File extenderBuildPlatformDir = new File(extenderBuildDir, platform.getExtenderPair());

        File extenderPrivacyManifest = new File(extenderBuildPlatformDir, privacyManifestFilename);
        if (extenderPrivacyManifest.exists()) {
            FileUtils.copyFile(extenderPrivacyManifest, targetPrivacyManifest);
        }
        else {
            IResource defaultPrivacyManifest = project.getResource(platform.getExtenderPaths()[0], "privacymanifest", false);
            if (defaultPrivacyManifest.exists()) {
                ExtenderUtil.writeResourceToFile(defaultPrivacyManifest, targetPrivacyManifest);
            }
        }
    }

    public void updateTemplateProperties() throws CompileExceptionError, IOException {
        String title = this.projectProperties.getStringValue("project", "title", "Unnamed");
        String exeName = BundleHelper.projectNameToBinaryName(title);
        this.templateProperties.put("exe-name", exeName);

        IBundler bundler = getOrCreateBundler();
        bundler.updateManifestProperties(project, platform, this.projectProperties, this.propertiesMap, this.templateProperties);
    }

  private File getAppManifestFile(Platform platform, File appDir) throws CompileExceptionError {
        IBundler bundler = getOrCreateBundler();
        String name = bundler.getMainManifestTargetPath(platform);
        return new File(appDir, name);
    }

    private String getMainManifestName(Platform platform) throws CompileExceptionError {
        IBundler bundler = getOrCreateBundler();
        return bundler.getMainManifestName(platform);
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
    private static Pattern resourceIssueLinkerUnresolvedSymbol = Pattern.compile("(?:(?:(?:\\/tmp\\/job[0-9]*\\/)?(?:upload\\/packages|upload|build)[\\/\\\\]))?([\\w.\\/\\\\]+)\\s*\\(([0-9]+)\\):?\\s*(error|warning|note|):?\\s*(.+)");

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
            Matcher m;
            String line = lines[count];

            m = resourceIssueLinkerUnresolvedSymbol.matcher(line);
            if (m.matches()) {
                issues.add(new BundleHelper.ResourceInfo(m.group(3), m.group(1), m.group(2), m.group(4)));
            }

            // Compare with some lookahead if it matches
            for (int i = 1; i <= 2 && (count+i) < lines.length; ++i) {
                line += "\n" + lines[count+i];
            }
            m = linkerPattern.matcher(line);
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
        } else {
            parseLogClang(lines, allIssues);
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
                throw new CompileExceptionError(iresource, -1, "Duplicate file in upload zip: " + resource.getPath());
            }
            uniquePaths.add(path);
        }
    }

    public static File buildEngineRemote(Project project, ExtenderClient extender, String platform, String sdkVersion, List<ExtenderResource> allSource, File logFile) throws ConnectException, NoHttpResponseException, CompileExceptionError, MultipleCompileException {
        File zipFile = null;

        try {
            zipFile = File.createTempFile("build_" + sdkVersion, ".zip");
            FileUtil.deleteOnExit(zipFile);
        } catch (IOException e) {
            throw new CompileExceptionError("Failed to create temp zip file", e.getCause());
        }

        checkForDuplicates(allSource);

        try {
            boolean async = true;
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

                    IResource appManifestResource = null;
                    IResource extManifestResource = null;

                    // For now, find the first ext.manifest
                    // Drawback of just picking one, is that build errors with no obvious "source", will be associated with the wrong ext.manifest
                    for (ExtenderResource r : allSource) {
                        if (extManifestResource == null && r.getPath().endsWith(ExtenderClient.extensionFilename)) {
                            extManifestResource = ExtenderUtil.getResource(r.getPath(), allSource);
                        }
                        if (appManifestResource == null && r.getPath().endsWith(".appmanifest")) {
                            appManifestResource = ExtenderUtil.getResource(r.getPath(), allSource);
                        }
                    }

                    IResource fallbackResource = extManifestResource;
                    if (fallbackResource == null)
                        fallbackResource = appManifestResource;
                    if (fallbackResource == null)
                        fallbackResource = project.getGameProjectResource();

                    IResource contextResource = null;

                    for (ResourceInfo info : issues) {

                        IResource issueResource = info.resource == null ? null : ExtenderUtil.getResource(info.resource, allSource);

                        // If it's the app manifest, let's translate it back into its original name
                        if (info.resource != null && info.resource.endsWith(ExtenderClient.appManifestFilename)) {
                            for (ExtenderResource extResource : allSource) {
                                if (((ExtenderUtil.FSAppManifestResource)extResource).getAbsPath().endsWith(info.resource)) {
                                    issueResource = ((ExtenderUtil.FSAppManifestResource)extResource).getResource();
                                    info.message = info.message.replace(extResource.getPath(), issueResource.getPath());
                                    break;
                                }
                            }
                        }

                        IResource exceptionResource = issueResource == null ? fallbackResource : issueResource;
                        int severity = info.severity.contains("error") ? Info.SEVERITY_ERROR : info.severity.equals("warning") ? Info.SEVERITY_WARNING : Info.SEVERITY_INFO;
                        exception.addIssue(severity, exceptionResource, info.message, info.lineNumber);

                        String msg = String.format("%s(%d): %s", info.resource != null ? info.resource : "<unknown>", info.lineNumber, info.message);

                        // The first resource generating errors should be related - we can use it to give context to the raw log.
                        if (contextResource == null && issueResource != null) {
                            contextResource = issueResource;
                        }
                    }

                    // If we do not yet have a context resource - fall back on another resource (possibly the wrong one!)
                    if (contextResource == null) {
                        contextResource = fallbackResource;
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

    public static boolean isArchiveIncluded(Project project) {
        return project.option("exclude-archive", "false").equals("false");
    }


    // must have two or more segments
    // each segment must start with a letter
    // all characters must be alphanumeric or an underscore
    public static boolean isValidAndroidPackageName(String packageName) {
        return packageName.matches("^[a-zA-Z][a-zA-Z0-9_]*(\\.[a-zA-Z][a-zA-Z0-9_]*)+$");
    }


    // must have two or more segments
    // each segment must start with a letter
    // all characters must be alphanumeric, an underscore or a hyphen
    public static boolean isValidAppleBundleIdentifier(String bundleIdentifier) {
        return bundleIdentifier.matches("^[a-zA-Z][a-zA-Z0-9_-]*(\\.[a-zA-Z][a-zA-Z0-9_-]*)+$");
    }




}
