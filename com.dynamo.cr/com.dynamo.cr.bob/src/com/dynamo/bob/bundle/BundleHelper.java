package com.dynamo.bob.bundle;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.StringWriter;
import java.net.URL;
import java.nio.file.FileSystem;
import java.nio.file.FileSystems;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.apache.commons.lang.StringUtils;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.io.filefilter.DirectoryFileFilter;
import org.apache.commons.io.filefilter.RegexFileFilter;

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
import com.dynamo.bob.pipeline.ExtenderUtil.JavaRExtenderResource;
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
    public static final String MANIFEST_NAME_OSX        = "Info.plist";
    public static final String MANIFEST_NAME_HTML5      = "engine_template.html";

    private static Logger logger = Logger.getLogger(BundleHelper.class.getName());

    public static void throwIfCanceled(ICanceled canceled) {
        if(canceled.isCanceled()) {
            throw new RuntimeException("Canceled");
        }
    }

    public BundleHelper(Project project, Platform platform, File bundleDir, String appDirSuffix, String variant) throws CompileExceptionError {
        this.projectProperties = project.getProjectProperties();

        this.project = project;
        this.platform = platform;
        this.title = this.projectProperties.getStringValue("project", "title", "Unnamed");

        this.buildDir = new File(project.getRootDirectory(), project.getBuildDirectory());
        this.appDir = new File(bundleDir, title + appDirSuffix);

        try {
            this.propertiesMap = createPropertiesMap(project.getProjectProperties());
        } catch (IOException e) {
            throw new CompileExceptionError(project.getGameProjectResource(), -1, e);
        }

        this.variant = variant;
    }

    public static String projectNameToBinaryName(String projectName) {
        String output = projectName.replaceAll("[^a-zA-Z0-9_]", "");
        if (output.equals("")) {
            return "dmengine";
        }
        return output;
    }

    static Object convert(String value, String type) {
        if (type != null && type.equals("bool")) {
            if (value != null) {
                return value.equals("1");
            } else {
                return false;
            }
        }
        return value;
    }

    public static Map<String, Map<String, Object>> createPropertiesMap(BobProjectProperties projectProperties) throws IOException {
        BobProjectProperties meta = new BobProjectProperties();
        InputStream is = Bob.class.getResourceAsStream("meta.properties");
        try {
            meta.load(is);
        } catch (ParseException e) {
            throw new RuntimeException("Failed to parse meta.properties", e);
        } finally {
            IOUtils.closeQuietly(is);
        }

        Map<String, Map<String, Object>> map = new HashMap<>();

        for (String c : meta.getCategoryNames()) {
            map.put(c, new HashMap<String, Object>());

            for (String k : meta.getKeys(c)) {
                if (k.endsWith(".default")) {
                    String k2 = k.split("\\.")[0];
                    String v = meta.getStringValue(c, k);
                    Object v2 = convert(v, meta.getStringValue(c, k2 + ".type"));
                    map.get(c).put(k2, v2);
                }
            }
        }

        for (String c : projectProperties.getCategoryNames()) {
            if (!map.containsKey(c)) {
                map.put(c, new HashMap<String, Object>());
            }

            for (String k : projectProperties.getKeys(c)) {
                String def = meta.getStringValue(c, k + ".default");
                map.get(c).put(k, def);
                String v = projectProperties.getStringValue(c, k);
                Object v2 = convert(v, meta.getStringValue(c, k + ".type"));
                map.get(c).put(k, v2);
            }
        }

        return map;
    }

    public IResource getResource(String category, String key) throws IOException {
        Map<String, Object> c = propertiesMap.get(category);
        if (c != null) {
            Object o = c.get(key);
            if (o != null && o instanceof String) {
                String s = (String)o;
                if (s != null && s.trim().length() > 0) {
                    return project.getResource(s);
                }
            }
        }
        throw new IOException(String.format("No resource found for %s.%s", category, key));
    }

    public String formatResource(Map<String, Object> properties, IResource resource) throws IOException {
        String data = new String(resource.getContent());
        Template template = Mustache.compiler().compile(data);
        StringWriter sw = new StringWriter();
        template.execute(this.propertiesMap, properties, sw);
        sw.flush();
        return sw.toString();
    }

    public BundleHelper format(Map<String, Object> properties, IResource resource, File toFile) throws IOException {
        FileUtils.write(toFile, formatResource(properties, resource));
        return this;
    }

    // Formats all the manifest files.
    // This is required for the manifest merger to not choke on the Mustasch patterns
    private List<File> formatAll(List<IResource> sources, File toDir, Map<String, Object> properties) throws IOException {
        List<File> out = new ArrayList<File>();
        for (IResource source : sources) {
            if (!source.exists()) {
                throw new IOException(String.format("Resource %s does not exist.", source));
            }
            // converts a relative path to something we can easily see if we get a merge failure: a/b/c.xml -> a_b_c.xml
            String name = source.getPath().replaceAll("[^a-zA-Z0-9_]", "_");
            File target = new File(toDir, name);
            format(properties, source, target);
            out.add(target);
        }
        return out;
    }

    public void mergeManifests(Map<String, Object> properties, IResource mainManifest, File outManifest) throws CompileExceptionError, IOException {
        String name;
        if (platform == Platform.Armv7Darwin || platform == Platform.Arm64Darwin) {
            name = BundleHelper.MANIFEST_NAME_IOS;
        } else if (platform == Platform.X86_64Darwin) {
            name = BundleHelper.MANIFEST_NAME_OSX;
        } else if (platform == Platform.Armv7Android || platform == Platform.Arm64Android) {
            name = BundleHelper.MANIFEST_NAME_ANDROID;
        } else if (platform == Platform.JsWeb || platform == Platform.WasmWeb) {
            name = BundleHelper.MANIFEST_NAME_HTML5;
        } else {
            throw new CompileExceptionError(mainManifest, -1, "Unsupported ManifestMergeTool platform: " + platform.toString());
        }

        // First, list all manifests
        List<IResource> sourceManifests = ExtenderUtil.getExtensionPlatformManifests(project, platform, name);
        // Put the main manifest in front
        sourceManifests.add(0, mainManifest);

        // Resolve all properties in each manifest
        File manifestDir = Files.createTempDirectory("manifests").toFile();
        List<File> resolvedManifests = formatAll(sourceManifests, manifestDir, properties);
        File resolvedMainManifest = resolvedManifests.get(0);
        // Remove the main manifest again
        resolvedManifests.remove(0);

        if (resolvedManifests.size() == 0) {
            // No need to merge a single file
            Files.copy(resolvedMainManifest.toPath(), outManifest.toPath(), StandardCopyOption.REPLACE_EXISTING);
            return;
        }

        ManifestMergeTool.Platform manifestPlatform;
        if (platform == Platform.Armv7Darwin || platform == Platform.Arm64Darwin) {
            manifestPlatform = ManifestMergeTool.Platform.IOS;
        } else if (platform == Platform.Armv7Android || platform == Platform.Arm64Android) {
            manifestPlatform = ManifestMergeTool.Platform.ANDROID;
        } else if (platform == Platform.X86_64Darwin) {
            manifestPlatform = ManifestMergeTool.Platform.OSX;
        } else if (platform == Platform.JsWeb || platform == Platform.WasmWeb) {
            manifestPlatform = ManifestMergeTool.Platform.HTML5;
        } else {
            throw new CompileExceptionError(mainManifest, -1, "Merging manifests for platform unsupported: " + platform.toString());
        }

        // Now merge these manifests in order (the main manifest is first)
        try {
            ManifestMergeTool.merge(manifestPlatform, resolvedMainManifest, outManifest, resolvedManifests);
        } catch (RuntimeException e) {
            throw new CompileExceptionError(mainManifest, -1, "Failed merging manifests: " + e.toString());
        }
        FileUtils.deleteDirectory(manifestDir);
    }

    private static Pattern aaptResourceErrorRe = Pattern.compile("^invalid resource directory name:\\s(.+)\\s(.+)\\s.*$", Pattern.MULTILINE);

    private List<ExtenderResource> generateRJava(List<String> resourceDirectories, List<String> extraPackages, File manifestFile, File apk, File outputDirectory) throws CompileExceptionError {

        try {
            Map<String, String> aaptEnv = new HashMap<String, String>();
            if (Platform.getHostPlatform() == Platform.X86_64Linux || Platform.getHostPlatform() == Platform.X86Linux) {
                aaptEnv.put("LD_LIBRARY_PATH", Bob.getPath(String.format("%s/lib", Platform.getHostPlatform().getPair())));
            }

            // Run aapt to generate R.java files
            List<String> args = new ArrayList<String>();
            args.add(Bob.getExe(Platform.getHostPlatform(), "aapt"));
            args.add("package");
            args.add("-f");
            args.add("--extra-packages");
            args.add(StringUtils.join(extraPackages, ":"));
            args.add("-m");
            args.add("--auto-add-overlay");
            args.add("-M"); args.add(manifestFile.getAbsolutePath());
            args.add("-I"); args.add(Bob.getPath("lib/android.jar"));
            args.add("-J"); args.add(outputDirectory.getAbsolutePath());
            if (apk != null) {
                args.add("-F"); args.add(apk.getAbsolutePath());
            }

            boolean debuggable = this.variant.equals(Bob.VARIANT_DEBUG) || Integer.parseInt(projectProperties.getStringValue("android", "debuggable", "0")) != 0;
            if (debuggable) {
                args.add("--debug-mode");
            }

            for( String s : resourceDirectories )
            {
                args.add("-S"); args.add(s);
            }

            Result res = Exec.execResultWithEnvironment(aaptEnv, args);

            if (res.ret != 0) {
                String msg = new String(res.stdOutErr);

                // Try our best to visualize the error from aapt
                Matcher m = aaptResourceErrorRe.matcher(msg);
                if (m.matches()) {
                    String path = m.group(1);
                    if (path.startsWith(project.getRootDirectory())) {
                        path = path.substring(project.getRootDirectory().length());
                    }
                    IResource r = project.getResource(FilenameUtils.concat(path, m.group(2))); // folder + filename
                    if (r != null) {
                        throw new CompileExceptionError(r, 1, String.format("Invalid Android resource folder name: '%s'\nSee https://developer.android.com/guide/topics/resources/providing-resources.html#table1 for valid directory names.\nAAPT Error: %s", m.group(2), msg));
                    }
                }
                throw new IOException(msg);
            }

        } catch (Exception e) {
            throw new CompileExceptionError(null, -1, "Failed building Android resources to R.java: " + e.getMessage());
        }

        List<ExtenderResource> extraSource = new ArrayList<ExtenderResource>();

        // Collect all *.java files from aapt output
        Collection<File> javaFiles = FileUtils.listFiles(
                outputDirectory,
                new RegexFileFilter(".+\\.java"),
                DirectoryFileFilter.DIRECTORY
        );

        // Add outputs as _app/rjava/ sources
        String rootRJavaPath = "_app/rjava/";
        for (File javaFile : javaFiles) {
            String relative = outputDirectory.toURI().relativize(javaFile.toURI()).getPath();
            String outputPath = rootRJavaPath + relative;
            extraSource.add(new JavaRExtenderResource(javaFile, outputPath));
        }
        return extraSource;
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

    public static void findAndroidAssetDirs(File dir, List<String> result) {
        if (!dir.isDirectory()) {
            return;
        }
        if (ExtenderUtil.matchesAndroidAssetDirectoryName(dir.getName())) {
            String parent = dir.getParentFile().getAbsolutePath();
            if (!result.contains(parent)) {
                result.add(parent);
            }
            return;
        }
        for (File file : dir.listFiles()) {
            findAndroidAssetDirs(file, result);
        }
    }

    public List<ExtenderResource> generateAndroidResources(Project project, File resDir, File manifestFile, File apk, File tmpDir) throws CompileExceptionError {

        // Get all Android specific resources needed to create R.java files
        try {
            BundleHelper.createAndroidResourceFolders(resDir);
            copyAndroidIcons(resDir);
        } catch (Exception e) {
            throw new CompileExceptionError(project.getGameProjectResource(), -1, e);
        }

        // We store the extensions' resources in a separate folder, because they otherwise failed on the Android naming convention.
        // I.e. resDir contains asset directories, extensionsDir contains package directories that contain asset directiores
        File extensionsDir = new File(tmpDir, "extensions");
        extensionsDir.mkdir();

        Map<String, IResource> resources = ExtenderUtil.getAndroidResources(project);
        ExtenderUtil.storeResources(extensionsDir, resources);

        Map<String, Object> bundleContext = null;
        {
            Map<String, Object> extensionContext = ExtenderUtil.getPlatformSettingsFromExtensions(project, "android");
            bundleContext = (Map<String, Object>)extensionContext.getOrDefault("bundle", null);
            if (bundleContext == null) {
                extensionContext = ExtenderUtil.getPlatformSettingsFromExtensions(project, Platform.Arm64Android.getPair());
                bundleContext = (Map<String, Object>)extensionContext.getOrDefault("bundle", null);
            }
            if (bundleContext == null) {
                extensionContext = ExtenderUtil.getPlatformSettingsFromExtensions(project, Platform.Armv7Android.getPair());
                bundleContext = (Map<String, Object>)extensionContext.getOrDefault("bundle", null);
            }
        }

        List<String> resourceDirectories = new ArrayList<>();
        findAndroidAssetDirs(extensionsDir, resourceDirectories);
        resourceDirectories.add(resDir.getAbsolutePath());

        // Run aapt to generate R.java files
        //     <tmpDir>/rjava - Output directory of aapt, all R.java files will be stored here
        File javaROutput = new File(tmpDir, "rjava");
        javaROutput.mkdir();

        List<String> extraPackages = new ArrayList<>();

        if (bundleContext != null) {
            List<String> excludePackages = (List<String>)bundleContext.getOrDefault("aaptExcludePackages", new ArrayList<String>());
            List<String> excludeResourceDirs = (List<String>)bundleContext.getOrDefault("aaptExcludeResourceDirs", new ArrayList<String>());

            extraPackages = excludeItems(extraPackages, excludePackages);
            resourceDirectories = excludeItems(resourceDirectories, excludeResourceDirs);

            extraPackages.addAll((List<String>)bundleContext.getOrDefault("aaptExtraPackages", new ArrayList<String>()));
        }

        return generateRJava(resourceDirectories, extraPackages, manifestFile, apk, javaROutput);
    }

    public BundleHelper copyBuilt(String name) throws IOException {
        FileUtils.copyFile(new File(buildDir, name), new File(appDir, name));
        return this;
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
        String[] iconPropNames = { "app_icon_57x57", "app_icon_72x72", "app_icon_76x76", "app_icon_114x114", "app_icon_120x120", "app_icon_144x144", "app_icon_152x152", "app_icon_167x167", "app_icon_180x180" };
        BufferedImage largestIconImage = getFallbackIconImage("ios", iconPropNames);

        // Copy game.project specified icons
        genIcon(largestIconImage, appDir, "ios",   "app_icon_57x57",       "Icon.png",  57);
        genIcon(largestIconImage, appDir, "ios", "app_icon_114x114",    "Icon@2x.png", 114);
        genIcon(largestIconImage, appDir, "ios",   "app_icon_72x72",    "Icon-72.png",  72);
        genIcon(largestIconImage, appDir, "ios", "app_icon_144x144", "Icon-72@2x.png", 144);
        genIcon(largestIconImage, appDir, "ios",   "app_icon_76x76",    "Icon-76.png",  76);
        genIcon(largestIconImage, appDir, "ios", "app_icon_152x152", "Icon-76@2x.png", 152);
        genIcon(largestIconImage, appDir, "ios", "app_icon_120x120", "Icon-60@2x.png", 120);
        genIcon(largestIconImage, appDir, "ios", "app_icon_180x180", "Icon-60@3x.png", 180);
        genIcon(largestIconImage, appDir, "ios", "app_icon_167x167",   "Icon-167.png", 167);
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

    private boolean copyIcon(BobProjectProperties projectProperties, String projectRoot, File resDir, String name, String outName)
            throws IOException {
        String resource = projectProperties.getStringValue("android", name);
        if (resource != null && resource.length() > 0) {
            File inFile = new File(projectRoot, resource);
            File outFile = new File(resDir, outName);
            FileUtils.copyFile(inFile, outFile);
            return true;
        }
        return false;
    }

    private boolean copyIconDPI(BobProjectProperties projectProperties, String projectRoot, File resDir, String name, String outName, String dpi)
            throws IOException {
            return copyIcon(projectProperties, projectRoot, resDir, name + "_" + dpi, "drawable-" + dpi + "/" + outName);
    }

    public Map<String, Object> createAndroidManifestProperties(String projectRoot, File resOutput, String exeName) throws IOException {

        Map<String, Object> properties = new HashMap<>();
        properties.put("exe-name", exeName);

        // We copy and resize the default icon in builtins if no other icons are set.
        // This means that the app will always have icons from now on.
        properties.put("has-icons?", true);

        // Copy push notification icons
        copyIcon(projectProperties, projectRoot, resOutput, "push_icon_small", "drawable/push_icon_small.png");
        copyIcon(projectProperties, projectRoot, resOutput, "push_icon_large", "drawable/push_icon_large.png");

        String[] dpis = new String[] { "ldpi", "mdpi", "hdpi", "xhdpi", "xxhdpi", "xxxhdpi" };
        for (String dpi : dpis) {
            copyIconDPI(projectProperties, projectRoot, resOutput, "push_icon_small", "push_icon_small.png", dpi);
            copyIconDPI(projectProperties, projectRoot, resOutput, "push_icon_large", "push_icon_large.png", dpi);
        }

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
    private static Pattern resourceIssueGCCRe = Pattern.compile("^(?:(?:(?:\\/tmp\\/job[0-9]*\\/)?(?:upload|build)\\/)|(?:.*\\/drive_c\\/))?([^:]+):([0-9]+):([0-9]*)?:?\\s*(error|warning|note|):?\\s*(.+)"); // GCC + Clang + Java
    private static Pattern resourceIssueCLRe = Pattern.compile("^(?:upload|build)\\/([^:]+)\\(([0-9]+)\\)([0-9]*):\\s*(fatal error|error|warning|note).*?:\\s*(.+)"); // CL.exe
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

    public static void parseLog(String platform, String log, List<ResourceInfo> issues) {
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

    public static void buildEngineRemote(ExtenderClient extender, String platform, String sdkVersion, List<ExtenderResource> allSource, File logFile, List<String> srcNames, List<File> outputEngines, File outputClassesDex, File proguardMapping) throws CompileExceptionError, MultipleCompileException {
        File zipFile = null;

        try {
            zipFile = File.createTempFile("build_" + sdkVersion, ".zip");
            zipFile.deleteOnExit();
        } catch (IOException e) {
            throw new CompileExceptionError("Failed to create temp zip file", e.getCause());
        }

        try {
            extender.build(platform, sdkVersion, allSource, zipFile, logFile);
        } catch (ExtenderClientException e) {
            String buildError = "<no log file>";
            if (logFile != null) {
                try {
                    List<ResourceInfo> issues = new ArrayList<>();
                    buildError = FileUtils.readFileToString(logFile);
                    parseLog(platform, buildError, issues);
                    MultipleCompileException exception = new MultipleCompileException("Build error", e);
                    IResource extManifestResource = ExtenderUtil.getResource(allSource.get(0).getPath(), allSource);
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

                    // If we do not yet have a context resource - fall back on an ext.manifest (possibly the wrong one!)
                    if (contextResource == null) {
                        contextResource = extManifestResource;
                    }

                    exception.attachLog(contextResource, buildError);
                    throw exception;
                } catch (IOException ioe) {
                    buildError = "<failed reading log>";
                }
            }
            buildError = String.format("'%s' could not be built. Sdk version: '%s'\nLog: '%s'", platform, sdkVersion, buildError);
            throw new CompileExceptionError(buildError, e.getCause());
        }

        FileSystem zip = null;
        try {
            zip = FileSystems.newFileSystem(zipFile.toPath(), null);
        } catch (IOException e) {
            throw new CompileExceptionError(String.format("Failed to mount temp zip file %s", zipFile.getAbsolutePath()), e.getCause());
        }

        // If we expect a classes.dex file, try to extract it from the zip
        if (outputClassesDex != null) {
            int nameindex = 1;
            while(true)
            {
                String name = nameindex == 1 ? "classes.dex" : String.format("classes%d.dex", nameindex);
                ++nameindex;

                File dex = new File(outputClassesDex.getParent(), name);
                try {
                    Path source = zip.getPath(name);
                    if (!Files.isReadable(source))
                        break;
                    try (FileOutputStream out = new FileOutputStream(dex)) {
                        Files.copy(source, out);
                    }
                } catch (IOException e) {
                    throw new CompileExceptionError(String.format("Failed to copy %s to %s", name, dex.getAbsolutePath()), e.getCause());
                }
            }
        }

        if (proguardMapping != null)
        {
            String name = "mapping.txt";
            try {
                Path source = zip.getPath(name);
                if (Files.isReadable(source)) {
                    try (FileOutputStream out = new FileOutputStream(proguardMapping)) {
                        Files.copy(source, out);
                    }
                }
            } catch (IOException e) {
                throw new CompileExceptionError(String.format("Failed to copy %s to %s", name, proguardMapping.getAbsolutePath()), e.getCause());
            }
        }

        for (int i = 0; i < srcNames.size(); i++) {
            String srcName = srcNames.get(i);
            File outputEngine = outputEngines.get(i);
            try {
                Path source = zip.getPath(srcName);
                try (FileOutputStream out = new FileOutputStream(outputEngine)) {
                    Files.copy(source, out);
                }
                outputEngine.setExecutable(true);
            } catch (IOException e) {
                throw new CompileExceptionError(String.format("Failed to copy %s to %s", srcName, outputEngine.getAbsolutePath()), e.getCause());
            }
        }
    }
}
