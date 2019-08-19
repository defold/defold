package com.dynamo.bob;

import static org.apache.commons.io.FilenameUtils.normalizeNoEndSeparator;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.net.ConnectException;
import java.net.HttpURLConnection;
import java.net.URISyntaxException;
import java.net.URL;
import java.net.URI;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.Enumeration;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.jar.Attributes;
import java.util.jar.JarFile;
import java.util.jar.Manifest;
import java.util.zip.ZipException;
import java.util.zip.ZipFile;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.codec.binary.Base64;

import com.defold.extender.client.ExtenderClient;
import com.defold.extender.client.ExtenderResource;

import com.dynamo.bob.archive.EngineVersion;
import com.dynamo.bob.archive.publisher.AWSPublisher;
import com.dynamo.bob.archive.publisher.DefoldPublisher;
import com.dynamo.bob.archive.publisher.NullPublisher;
import com.dynamo.bob.archive.publisher.Publisher;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.archive.publisher.ZipPublisher;

import com.dynamo.bob.bundle.AndroidBundler;
import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.bundle.HTML5Bundler;
import com.dynamo.bob.bundle.IBundler;
import com.dynamo.bob.bundle.IOSBundler;
import com.dynamo.bob.bundle.LinuxBundler;
import com.dynamo.bob.bundle.OSXBundler;
import com.dynamo.bob.bundle.Win32Bundler;
import com.dynamo.bob.bundle.Win64Bundler;
import com.dynamo.bob.fs.ClassLoaderMountPoint;
import com.dynamo.bob.fs.FileSystemWalker;
import com.dynamo.bob.fs.IFileSystem;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ZipMountPoint;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.LibraryUtil;
import com.dynamo.bob.util.ReportGenerator;
import com.dynamo.graphics.proto.Graphics.TextureProfiles;

/**
 * Project abstraction. Contains input files, builder, tasks, etc
 * @author Christian Murray
 *
 */
public class Project {

    public final static String LIB_DIR = ".internal/lib";
    public final static String CACHE_DIR = ".internal/cache";

    public enum OutputFlags {
        NONE,
        UNCOMPRESSED
    }

    private IFileSystem fileSystem;
    private Map<String, Class<? extends Builder<?>>> extToBuilder = new HashMap<String, Class<? extends Builder<?>>>();
    private List<String> inputs = new ArrayList<String>();
    private HashMap<String, EnumSet<OutputFlags>> outputs = new HashMap<String, EnumSet<OutputFlags>>();
    private ArrayList<Task<?>> newTasks;
    private State state;
    private String rootDirectory = ".";
    private String buildDirectory = "build";
    private Map<String, String> options = new HashMap<String, String>();
    private List<URL> libUrls = new ArrayList<URL>();
    private final List<String> excludedCollectionProxies = new ArrayList<String>();
    private List<String> propertyFiles = new ArrayList<String>();

    private BobProjectProperties projectProperties;
    private Publisher publisher;

    private TextureProfiles textureProfiles;

    public Project(IFileSystem fileSystem) {
        this.fileSystem = fileSystem;
        this.fileSystem.setRootDirectory(rootDirectory);
        this.fileSystem.setBuildDirectory(buildDirectory);
        clearProjectProperties();
    }

    public Project(IFileSystem fileSystem, String sourceRootDirectory, String buildDirectory) {
        this.rootDirectory = normalizeNoEndSeparator(new File(sourceRootDirectory).getAbsolutePath(), true);
        this.buildDirectory = normalizeNoEndSeparator(buildDirectory, true);
        this.fileSystem = fileSystem;
        this.fileSystem.setRootDirectory(this.rootDirectory);
        this.fileSystem.setBuildDirectory(this.buildDirectory);
        clearProjectProperties();
    }

    public void dispose() {
        this.fileSystem.close();
    }

    public String getRootDirectory() {
        return rootDirectory;
    }

    public String getBuildDirectory() {
        return buildDirectory;
    }

    public String getLibPath() {
        return FilenameUtils.concat(this.rootDirectory, LIB_DIR);
    }

    public String getBuildCachePath() {
        return FilenameUtils.concat(this.rootDirectory, CACHE_DIR);
    }

    public BobProjectProperties getProjectProperties() {
        return projectProperties;
    }

    public void setPublisher(Publisher publisher) {
        this.publisher = publisher;
    }

    public Publisher getPublisher() {
        return this.publisher;
    }

    /**
     * Scan package for builder classes
     * @param scanner class scanner
     * @param pkg package name to be scanned
     */
    public void scan(IClassScanner scanner, String pkg) {
        Set<String> classNames = scanner.scan(pkg);
        doScan(classNames);
    }

    private static String getManifestInfo(String attribute) {
        Enumeration resEnum;
        try {
            resEnum = Thread.currentThread().getContextClassLoader().getResources(JarFile.MANIFEST_NAME);
            while (resEnum.hasMoreElements()) {
                try {
                    URL url = (URL)resEnum.nextElement();
                    InputStream is = url.openStream();
                    if (is != null) {
                        Manifest manifest = new Manifest(is);
                        Attributes mainAttribs = manifest.getMainAttributes();
                        String value = mainAttribs.getValue(attribute);
                        if(value != null) {
                            return value;
                        }
                    }
                }
                catch (Exception e) {
                    // Silently ignore wrong manifests on classpath?
                }
            }
        } catch (IOException e1) {
            // Silently ignore wrong manifests on classpath?
        }
        return null;
    }

    @SuppressWarnings("unchecked")
    private void doScan(Set<String> classNames) {
        boolean is_bob_light = getManifestInfo("is-bob-light") != null;

        for (String className : classNames) {
            // Ignore TexcLibrary to avoid it being loaded and initialized
            // We're also skipping some of the bundler classes, since we're only building content,
            // not doing bundling when using bob-light
            boolean skip = className.startsWith("com.dynamo.bob.TexcLibrary") ||
                    (is_bob_light && className.startsWith("com.dynamo.bob.archive.publisher.AWSPublisher")) ||
                    (is_bob_light && className.startsWith("com.dynamo.bob.pipeline.ExtenderUtil")) ||
                    (is_bob_light && className.startsWith("com.dynamo.bob.bundle.ManifestMergeTool")) ||
                    (is_bob_light && className.startsWith("com.dynamo.bob.bundle.BundleHelper"));
            if (!skip) {
                try {
                    Class<?> klass = Class.forName(className);
                    BuilderParams params = klass.getAnnotation(BuilderParams.class);
                    if (params != null) {
                        for (String inExt : params.inExts()) {
                            extToBuilder.put(inExt, (Class<? extends Builder<?>>) klass);
                        }
                    }
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        }
    }

    private Task<?> doCreateTask(String input) throws CompileExceptionError {
        Class<? extends Builder<?>> builderClass = getBuilderFromExtension(input);
        if (builderClass != null) {
            return doCreateTask(input, builderClass);
        } else {
            logWarning("No builder for '%s' found", input);
        }
        return null;
    }

    private Task<?> doCreateTask(String input, Class<? extends Builder<?>> builderClass) throws CompileExceptionError {
        Builder<?> builder;
        try {
            builder = builderClass.newInstance();
            builder.setProject(this);
            IResource inputResource = fileSystem.get(input);
            Task<?> task = builder.create(inputResource);
            return task;
        } catch (CompileExceptionError e) {
            // Just pass CompileExceptionError on unmodified
            throw e;
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private Class<? extends Builder<?>> getBuilderFromExtension(String input) {
        String ext = "." + FilenameUtils.getExtension(input);
        Class<? extends Builder<?>> builderClass = extToBuilder.get(ext);
        return builderClass;
    }

    /**
     * Create task from resource. Typically called from builder
     * that create intermediate output/input-files
     * @param input input resource
     * @return task
     * @throws CompileExceptionError
     */
    public Task<?> buildResource(IResource input) throws CompileExceptionError {
        Class<? extends Builder<?>> builderClass = getBuilderFromExtension(input.getPath());
        if (builderClass == null) {
            logWarning("No builder for '%s' found", input);
            return null;
        }

        Task<?> task = doCreateTask(input.getPath(), builderClass);
        if (task != null) {
            newTasks.add(task);
        }
        return task;
    }

    /**
     * Create task from resource with explicit builder.
     * @param input input resource
     * @param builderClass class to build resource with
     * @return
     * @throws CompileExceptionError
     */
    public Task<?> buildResource(IResource input, Class<? extends Builder<?>> builderClass) throws CompileExceptionError {
        Task<?> task = doCreateTask(input.getPath(), builderClass);
        if (task != null) {
            newTasks.add(task);
        }
        return task;
    }

    private List<String> sortInputs() {
        ArrayList<String> sortedInputs = new ArrayList<String>(inputs);
        Collections.sort(sortedInputs, new Comparator<String>() {

            @Override
            public int compare(String i1, String i2) {
                Class<? extends Builder<?>> b1 = getBuilderFromExtension(i1);
                Class<? extends Builder<?>> b2 = getBuilderFromExtension(i2);

                BuilderParams p1 = b1.getAnnotation(BuilderParams.class);
                BuilderParams p2 = b2.getAnnotation(BuilderParams.class);

                return p1.createOrder() - p2.createOrder();
            }
        });
        return sortedInputs;
    }

    private void createTasks() throws CompileExceptionError {
        newTasks = new ArrayList<Task<?>>();
        List<String> sortedInputs = sortInputs();

        for (String input : sortedInputs) {
            Task<?> task = doCreateTask(input);
            if (task != null) {
                newTasks.add(task);
            }
        }
    }

    private void logWarning(String fmt, Object... args) {
        System.err.println(String.format(fmt, args));
    }

    public void createPublisher(boolean shouldPublish) throws CompileExceptionError {
        try {
            String settingsPath = this.getProjectProperties().getStringValue("liveupdate", "settings", "/liveupdate.settings"); // if no value set use old hardcoded path (backward compatability)
            IResource publisherSettings = this.fileSystem.get(settingsPath);
            if (!publisherSettings.exists()) {
                if (shouldPublish) {
                    IResource gameProject = this.fileSystem.get("/game.project");
                    throw new CompileExceptionError(gameProject, 0, "There is no liveupdate.settings file specified in game.project or the file is missing from disk.");
                } else {
                    this.publisher = new NullPublisher(new PublisherSettings());
                }
            } else {
                ByteArrayInputStream is = new ByteArrayInputStream(publisherSettings.getContent());
                PublisherSettings settings = PublisherSettings.load(is);
                if (shouldPublish) {
                    if (PublisherSettings.PublishMode.Amazon.equals(settings.getMode())) {
                        this.publisher = new AWSPublisher(settings);
                    } else if (PublisherSettings.PublishMode.Defold.equals(settings.getMode())) {
                        this.publisher = new DefoldPublisher(settings);
                    } else if (PublisherSettings.PublishMode.Zip.equals(settings.getMode())) {
                        this.publisher = new ZipPublisher(settings);
                    } else {
                        throw new CompileExceptionError("The publisher specified is not supported", null);
                    }
                } else {
                    this.publisher = new NullPublisher(settings);
                }
            }
        } catch (CompileExceptionError e) {
            throw e;
        } catch (Throwable e) {
            throw new CompileExceptionError(null, 0, e.getMessage(), e);
        }
    }

    public void clearProjectProperties() {
        projectProperties = new BobProjectProperties();
    }

    public static void loadPropertyFile(BobProjectProperties properties, String filepath) throws IOException {
        Path pathHandle = Paths.get(filepath);
        if (!Files.exists(pathHandle) || !pathHandle.toFile().isFile())
            throw new IOException(filepath + " is not a file");
        byte[] data = Files.readAllBytes(pathHandle);
        ByteArrayInputStream is = new ByteArrayInputStream(data);
        try {
            properties.load(is);
        } catch(ParseException e) {
            throw new IOException("Could not parse: " + filepath);
        }
    }

    // Loads the properties from a game project settings file
    // Also adds any properties specified with the "--settings" flag
    public static BobProjectProperties loadProperties(IResource resource, List<String> settingsFiles) throws IOException {
        if (!resource.exists()) {
            throw new IOException(String.format("Project file not found: %s", resource.getAbsPath()));
        }

        BobProjectProperties properties = new BobProjectProperties();
        try {
            properties.loadDefaults();
            Project.loadPropertyFile(properties, resource.getAbsPath());
        } catch(ParseException e) {
            throw new IOException("Could not parse: " + resource.getAbsPath());
        }

        for (String filepath : settingsFiles) {
            Project.loadPropertyFile(properties, filepath);
        }

        return properties;
    }

    public void loadProjectFile() throws IOException, ParseException {
        IResource gameProject = getGameProjectResource();
        if (gameProject.exists()) {
            projectProperties = Project.loadProperties(gameProject, this.getPropertyFiles());
        }
    }

    public void addPropertyFile(String filepath) {
        propertyFiles.add(filepath);
    }

    // Returns the command line specified property files
    public List<String> getPropertyFiles() {
        return propertyFiles;
    }

    private void logExceptionToStdErr(IResource res, int line)
    {
        String resourceString = "unspecified";
        String resourceLineString = "";
        if (res != null) {
            resourceString = res.toString();
        }
        if (line > 0) {
            resourceLineString = String.format(" at line %d", line);
        }
        System.err.println("Error in resource: " + resourceString + resourceLineString);
    }

    /**
     * Build the project
     * @param monitor
     * @return list of {@link TaskResult}. Only executed nodes are part of the list.
     * @throws IOException
     * @throws CompileExceptionError
     */
    public List<TaskResult> build(IProgress monitor, String... commands) throws IOException, CompileExceptionError, MultipleCompileException {
        try {
            loadProjectFile();
            return doBuild(monitor, commands);
        } catch (CompileExceptionError e) {
            logExceptionToStdErr(e.getResource(), e.getLineNumber());
            // Pass on unmodified
            throw e;
        } catch (MultipleCompileException e) {
            logExceptionToStdErr(e.getContextResource(), -1);
            // Pass on unmodified
            throw e;
        } catch (Throwable e) {
            throw new CompileExceptionError(null, 0, e.getMessage(), e);
        }
    }

    /**
     * Mounts all the mount point associated with the project.
     * @param resourceScanner scanner to use for finding resources in the java class path
     * @throws IOException
     * @throws CompileExceptionError
     */
    public void mount(IResourceScanner resourceScanner) throws IOException, CompileExceptionError {
        this.fileSystem.clearMountPoints();
        this.fileSystem.addMountPoint(new ClassLoaderMountPoint(this.fileSystem, "builtins/**", resourceScanner));
        List<File> libFiles = LibraryUtil.convertLibraryUrlsToFiles(getLibPath(), this.libUrls);
        boolean missingFiles = false;
        for (File file : libFiles) {
            if (file.exists()) {
                this.fileSystem.addMountPoint(new ZipMountPoint(this.fileSystem, file.getAbsolutePath()));
            } else {
                missingFiles = true;
            }
        }
        if (missingFiles) {
            logWarning("Some libraries could not be found locally, use the resolve command to fetch them.");
        }
    }

    /**
     * Match resource name by resource list. Comparison is case-insensitive and stripped by resource path and extension.
     * @param resource resource
     * @param resourceList list of resources to match to resource
     * @return matching resource in resource list, or null.
     */
    private IResource getMatchingResourceByName(IResource resource, List<IResource> resourceList) {
        String resourceName = FilenameUtils.removeExtension(FilenameUtils.getBaseName(resource.toString())).toLowerCase();
        for (IResource input : resourceList) {
            if (FilenameUtils.removeExtension(FilenameUtils.getBaseName(input.toString())).toLowerCase().equals(resourceName)) {
                return input;
            }
        }
        return null;
    }

    /**
     * Get the conflicting output resource given two list of input resources.
     * If no direct resource can be determined, try finding a matching resource by name in either input resource list.
     * Finally, failing that, resort to the first resource in primary input resource list.
     * @param output output resource
     * @param inputs1 primary resource list of input resource filenames
     * @param inputs2 secondary resource list of input resource filenames
     * @return conflicting resource
     */
    private IResource getConflictingResource(IResource output, List<IResource> inputs1, List<IResource> inputs2) {
        if (inputs1.size() == 1) {
            return inputs1.get(0);
        } else if (inputs2.size() == 1) {
            return inputs2.get(0);
        }
        IResource resource = getMatchingResourceByName(output, inputs1);
        if(resource != null){
            return resource;
        }
        resource = getMatchingResourceByName(output, inputs2);
        if(resource != null){
            return resource;
        }
        return inputs1.get(0);
    }

    /**
     * Validate there are no conflicting input resources for any given output
     * resource. If any output resource exists more than once in the list of
     * build output tasks, there is a conflict.
     * @throws CompileExceptionError
     */
    private void validateBuildResourceMapping() throws CompileExceptionError {
        Map<String, List<IResource>> build_map = new HashMap<String, List<IResource>>();
        for (Task<?> t : this.newTasks) {
            List<IResource> inputs = t.getInputs();
            List<IResource> outputs = t.getOutputs();
            for (IResource output : outputs) {
                String outStr = output.toString();
                if (build_map.containsKey(outStr)) {
                    List<IResource> inputsStored = build_map.get(outStr);
                    String errMsg = "Conflicting output resource '" + outStr + "‘ generated by the following input files: " + inputs.toString() + " <-> " + inputsStored.toString();
                    IResource errRes = getConflictingResource(output, inputs, inputsStored);
                    throw new CompileExceptionError(errRes, 0, errMsg);
                }
                build_map.put(outStr, inputs);
            }
        }
    }


    static Map<Platform, Class<? extends IBundler>> bundlers;
    static {
        bundlers = new HashMap<>();
        bundlers.put(Platform.X86_64Darwin, OSXBundler.class);
        bundlers.put(Platform.X86_64Linux, LinuxBundler.class);
        bundlers.put(Platform.X86Win32, Win32Bundler.class);
        bundlers.put(Platform.X86_64Win32, Win64Bundler.class);
        bundlers.put(Platform.Armv7Android, AndroidBundler.class);
        bundlers.put(Platform.Armv7Darwin, IOSBundler.class);
        bundlers.put(Platform.X86_64Ios, IOSBundler.class);
        bundlers.put(Platform.JsWeb, HTML5Bundler.class);
    }

    private void bundle(IProgress monitor) throws IOException, CompileExceptionError {
        IProgress m = monitor.subProgress(1);
        m.beginTask("Bundling...", 1);
        String pair = option("platform", null);
        if (pair == null) {
            throw new CompileExceptionError(null, -1, "No platform specified");
        }

        Platform platform = Platform.get(pair);
        if (platform == null) {
            throw new CompileExceptionError(null, -1, String.format("Platform %s not supported", pair));
        }

        Class<? extends IBundler> bundlerClass = bundlers.get(platform);
        if (bundlerClass == null) {
            throw new CompileExceptionError(null, -1, String.format("Platform %s not supported", pair));
        }

        IBundler bundler;
        try {
            bundler = bundlerClass.newInstance();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        String bundleOutput = option("bundle-output", null);
        File bundleDir = null;
        if (bundleOutput != null) {
            bundleDir = new File(bundleOutput);
        } else {
            bundleDir = new File(getRootDirectory(), getBuildDirectory());
        }
        BundleHelper.throwIfCanceled(monitor);
        bundleDir.mkdirs();
        bundler.bundleApplication(this, bundleDir, monitor);
        m.worked(1);
        m.done();
    }

    private static boolean anyFailing(Collection<TaskResult> results) {
        for (TaskResult taskResult : results) {
            if (!taskResult.isOk()) {
                return true;
            }
        }

        return false;
    }

    public Platform getPlatform() throws CompileExceptionError {
        String pair = option("platform", null);
        Platform p = Platform.getHostPlatform();
        if (pair != null) {
            p = Platform.get(pair);
        }

        if (p == null) {
            throw new CompileExceptionError(null, -1, String.format("Platform %s not supported", pair));
        }

        return p;
    }

    public String[] getPlatformStrings() throws CompileExceptionError {
        Platform p = getPlatform();
        PlatformArchitectures platformArchs = p.getArchitectures();
        String[] platformStrings;
        if (p == Platform.Armv7Darwin || p == Platform.Arm64Darwin || p == Platform.JsWeb || p == Platform.WasmWeb || p == Platform.Armv7Android || p == Platform.Arm64Android)
        {
            // Here we'll get a list of all associated architectures (armv7, arm64) and build them at the same time
            platformStrings = platformArchs.getArchitectures();
        }
        else
        {
            platformStrings = new String[1];
            platformStrings[0] = p.getPair();
        }
        return platformStrings;
    }

    public void buildEngine(IProgress monitor, String[] architectures, Map<String,String> appmanifestOptions) throws IOException, CompileExceptionError, MultipleCompileException {

        // Store the engine one level above the content build since that folder gets removed during a distclean
        String internalDir = FilenameUtils.concat(rootDirectory, ".internal");
        File cacheDir = new File(FilenameUtils.concat(internalDir, "cache"));
        cacheDir.mkdirs();

        String serverURL = this.option("build-server", "https://build.defold.com");

        // Get SHA1 and create log file
        final String sdkVersion = this.option("defoldsdk", EngineVersion.sha1);
        File logFile = File.createTempFile("build_" + sdkVersion + "_", ".txt");
        logFile.deleteOnExit();

        IProgress m = monitor.subProgress(architectures.length);
        m.beginTask("Building engine...", 0);

        final String variant = appmanifestOptions.get("baseVariant");

        // Build all skews of platform
        boolean androidResourcesGenerated = false;
        String outputDir = options.getOrDefault("binary-output", FilenameUtils.concat(rootDirectory, "build"));
        for (int i = 0; i < architectures.length; ++i) {
            Platform platform = Platform.get(architectures[i]);

            String buildPlatform = platform.getExtenderPair();
            File buildDir = new File(FilenameUtils.concat(outputDir, buildPlatform));
            buildDir.mkdirs();

            List<String> defaultNames = platform.formatBinaryName("dmengine");
            List<File> exes = new ArrayList<File>();
            for (String name : defaultNames) {
                File exe = new File(FilenameUtils.concat(buildDir.getAbsolutePath(), name));
                exes.add(exe);
            }

            List<ExtenderResource> allSource = ExtenderUtil.getExtensionSources(this, platform, appmanifestOptions);

            File classesDexFile = null;
            File proguardMappingFile = null;
            File tmpDir = null;

            if (platform.equals(Platform.Armv7Android) || platform.equals(Platform.Arm64Android)) {
                // If we are building for Android, We output a mapping file per architechture
                // so that the user can select which of the mapping files are more suitable,
                // since they can diff between architechtures
                String mappingsName = "mapping-" + (platform.equals(Platform.Armv7Android) ? "armv7" : "arm64") + ".txt";
                proguardMappingFile = new File(FilenameUtils.concat(buildDir.getAbsolutePath(), mappingsName));

                // NOTE:
                // We previously only generated and sent Android resources for at most one arch,
                // to avoid sending and building these twice.
                // However the server will run proguard for both of these architectures which means
                // for the second arch it will not find R attributes and fail.
                // if (!androidResourcesGenerated)
                {
                    androidResourcesGenerated = true;

                    Bob.initAndroid(); // extract resources

                    // If we are building for Android, we expect a classes.dex file to be returned as well.
                    classesDexFile = new File(FilenameUtils.concat(buildDir.getAbsolutePath(), "classes.dex"));

                    List<String> resDirs = new ArrayList<>();
                    List<String> extraPackages = new ArrayList<>();

                    // Create temp files and directories needed to run aapt and output R.java files
                    tmpDir = Files.createTempDirectory("bob_bundle_tmp").toFile();
                    tmpDir.mkdirs();

                    // <tmpDir>/res - Where to collect all resources needed for aapt
                    File resDir = new File(tmpDir, "res");
                    resDir.mkdir();

                    String title = projectProperties.getStringValue("project", "title", "Unnamed");
                    String exeName = BundleHelper.projectNameToBinaryName(title);

                    BundleHelper helper = new BundleHelper(this, platform, tmpDir, "", variant);

                    File manifestFile = new File(tmpDir, "AndroidManifest.xml"); // the final, merged manifest
                    IResource sourceManifestFile = helper.getResource("android", "manifest");

                    Map<String, Object> properties = helper.createAndroidManifestProperties(this.getRootDirectory(), resDir, exeName);
                    helper.mergeManifests(properties, sourceManifestFile, manifestFile);

                    BundleHelper.throwIfCanceled(monitor);

                    List<ExtenderResource> extraSource = helper.generateAndroidResources(this, resDir, manifestFile, null, tmpDir);
                    allSource.addAll(extraSource);
                }
            }

            ExtenderClient extender = new ExtenderClient(serverURL, cacheDir);
            BundleHelper.buildEngineRemote(extender, buildPlatform, sdkVersion, allSource, logFile, defaultNames, exes, classesDexFile, proguardMappingFile);

            if (tmpDir != null) {
                FileUtils.deleteDirectory(tmpDir);
            }

            m.worked(1);
        }

        m.done();
    }

    private void cleanEngine(IProgress monitor, String[] platformStrings) throws IOException, CompileExceptionError {
        IProgress m = monitor.subProgress(platformStrings.length);
        m.beginTask("Cleaning engine...", 0);

        String outputDir = options.getOrDefault("binary-output", FilenameUtils.concat(rootDirectory, "build"));
        for (int i = 0; i < platformStrings.length; ++i) {
            Platform platform = Platform.get(platformStrings[i]);

            String buildPlatform = platform.getExtenderPair();
            File buildDir = new File(FilenameUtils.concat(outputDir, buildPlatform));
            if (!buildDir.exists()) {
                continue;
            }

            List<String> defaultNames = platform.formatBinaryName("dmengine");
            for (String defaultName : defaultNames) {
                File exe = new File(FilenameUtils.concat(buildDir.getAbsolutePath(), defaultName));
                if (exe.exists()) {
                    exe.delete();
                }
            }

            // If we are building for Android, we expect a classes.dex file to be returned as well.
            if (platform.equals(Platform.Armv7Android) || platform.equals(Platform.Arm64Android)) {
                int nameindex = 1;
                while(true)
                {
                    String name = nameindex == 1 ? "classes.dex" : String.format("classes%d.dex", nameindex);
                    ++nameindex;

                    File classesDexFile = new File(FilenameUtils.concat(buildDir.getAbsolutePath(), name));
                    if (classesDexFile.exists()) {
                        classesDexFile.delete();
                    } else {
                        break;
                    }
                }
            }

            m.worked(1);
        }

        m.done();
    }

    private List<TaskResult> doBuild(IProgress monitor, String... commands) throws IOException, CompileExceptionError, MultipleCompileException {
        fileSystem.loadCache();
        IResource stateResource = fileSystem.get(FilenameUtils.concat(buildDirectory, "state"));
        state = State.load(stateResource);
        createTasks();
        validateBuildResourceMapping();
        List<TaskResult> result = new ArrayList<TaskResult>();

        BundleHelper.throwIfCanceled(monitor);

        monitor.beginTask("", 100);

        loop:
        for (String command : commands) {
            BundleHelper.throwIfCanceled(monitor);
            switch (command) {
                case "build": {
                    ExtenderUtil.checkProjectForDuplicates(this); // Throws if there are duplicate files in the project (i.e. library and local files conflict)

                    // Do early test if report files are writable before we start building
                    boolean generateReport = this.hasOption("build-report") || this.hasOption("build-report-html");
                    FileWriter fileJSONWriter = null;
                    FileWriter fileHTMLWriter = null;

                    if (this.hasOption("build-report")) {
                        String reportJSONPath = this.option("build-report", "report.json");
                        File reportJSONFile = new File(reportJSONPath);
                        fileJSONWriter = new FileWriter(reportJSONFile);
                    }
                    if (this.hasOption("build-report-html")) {
                        String reportHTMLPath = this.option("build-report-html", "report.html");
                        File reportHTMLFile = new File(reportHTMLPath);
                        fileHTMLWriter = new FileWriter(reportHTMLFile);
                    }

                    IProgress m = monitor.subProgress(99);
                    BundleHelper.throwIfCanceled(monitor);
                    m.beginTask("Building...", newTasks.size());
                    result = runTasks(m);
                    m.done();
                    if (anyFailing(result)) {
                        break loop;
                    }
                    BundleHelper.throwIfCanceled(monitor);

                    final String[] platforms = getPlatformStrings();
                    // Get or build engine binary
                    boolean buildRemoteEngine = ExtenderUtil.hasNativeExtensions(this);
                    if (buildRemoteEngine) {

                        final String variant = this.option("variant", Bob.VARIANT_RELEASE);
                        final Boolean withSymbols = this.hasOption("with-symbols");

                        Map<String, String> appmanifestOptions = new HashMap<>();
                        appmanifestOptions.put("baseVariant", variant);
                        appmanifestOptions.put("withSymbols", withSymbols.toString());

                        // Since this can be a call from Editor we can't expect the architectures option to be set.
                        // We default to the default architectures for the platform, and take the option value
                        // only if it has been set.
                        Platform platform = this.getPlatform();
                        String[] architectures = platform.getArchitectures().getDefaultArchitectures();
                        String customArchitectures = this.option("architectures", null);
                        if (customArchitectures != null) {
                            architectures = customArchitectures.split(",");
                        }

                        buildEngine(monitor, architectures, appmanifestOptions);
                    } else {
                        // Remove the remote built executables in the build folder, they're still in the cache
                        cleanEngine(monitor, platforms);
                    }

                    BundleHelper.throwIfCanceled(monitor);

                    // Generate and save build report
                    if (generateReport) {
                        IProgress mrep = monitor.subProgress(1);
                        mrep.beginTask("Generating report...", 1);
                        ReportGenerator rg = new ReportGenerator(this);
                        String reportJSON = rg.generateJSON();

                        // Save JSON report
                        if (this.hasOption("build-report")) {
                            fileJSONWriter.write(reportJSON);
                            fileJSONWriter.close();
                        }

                        // Save HTML report
                        if (this.hasOption("build-report-html")) {
                            String reportHTML = rg.generateHTML(reportJSON);
                            fileHTMLWriter.write(reportHTML);
                            fileHTMLWriter.close();
                        }
                        mrep.done();
                    }

                    break;
                }
                case "clean": {
                    IProgress m = monitor.subProgress(1);
                    m.beginTask("Cleaning...", newTasks.size());
                    for (Task<?> t : newTasks) {
                        List<IResource> outputs = t.getOutputs();
                        for (IResource r : outputs) {
                            BundleHelper.throwIfCanceled(monitor);
                            r.remove();
                            m.worked(1);
                        }
                    }
                    m.done();
                    break;
                }
                case "distclean": {
                    IProgress m = monitor.subProgress(1);
                    m.beginTask("Cleaning...", newTasks.size());
                    BundleHelper.throwIfCanceled(monitor);
                    FileUtils.deleteDirectory(new File(FilenameUtils.concat(rootDirectory, buildDirectory)));
                    m.worked(1);
                    m.done();
                    break;
                }
                case "bundle": {
                    bundle(monitor);
                    break;
                }
                default: break;
            }
        }

        monitor.done();
        state.save(stateResource);
        fileSystem.saveCache();
        return result;
    }

    @SuppressWarnings({ "rawtypes", "unchecked" })
    private List<TaskResult> runTasks(IProgress monitor) throws IOException {

        // set of all completed tasks. The set includes both task run
        // in this session and task already completed (output already exists with correct signatures, see below)
        // the set also contains failed tasks
        Set<Task> completedTasks = new HashSet<>();

        // the set of all output files generated
        // in this or previous session
        Set<IResource> completedOutputs = new HashSet<>();

        List<TaskResult> result = new ArrayList<>();

        List<Task<?>> tasks = new ArrayList<>(newTasks);
        // set of *all* possible output files
        Set<IResource> allOutputs = new HashSet<>();
        for (Task<?> task : newTasks) {
            allOutputs.addAll(task.getOutputs());
        }
        newTasks.clear();

        // Keep track of the paths for all outputs
        outputs = new HashMap<>(allOutputs.size());
        for (IResource res : allOutputs) {
            outputs.put(res.getAbsPath(), EnumSet.noneOf(OutputFlags.class));
        }

        // This flag is set to true as soon as one task has failed. This will
        // break out of the outer loop after the remaining tasks has been tried once.
        // NOTE The underlying problem is that if a task fails and has dependent
        // tasks, the dependent tasks will be tried forever. It should be solved
        // by marking all dependent tasks as failed instead of this flag.
        boolean taskFailed = false;
run:
        while (completedTasks.size() < tasks.size()) {
            for (Task<?> task : tasks) {
                BundleHelper.throwIfCanceled(monitor);
                // deps are the task input files generated by another task not yet completed,
                // i.e. "solve" the dependency graph
                Set<IResource> deps = new HashSet<>(task.getInputs());
                deps.retainAll(allOutputs);
                deps.removeAll(completedOutputs);
                if (deps.size() > 0) {
                    // postpone task. dependent input not yet generated
                    continue;
                }

                monitor.worked(1);

                byte[] taskSignature = task.calculateSignature(this);

                // do all output files exist?
                boolean allOutputExists = true;
                for (IResource r : task.getOutputs()) {
                    if (!r.exists()) {
                        allOutputExists = false;
                        break;
                    }
                }

                // compare all task signature. current task signature between previous
                // signature from state on disk
                List<byte[]> outputSigs = new ArrayList<byte[]>();
                for (IResource r : task.getOutputs()) {
                    byte[] s = state.getSignature(r.getAbsPath());
                    outputSigs.add(s);
                }
                boolean allSigsEquals = true;
                for (byte[] sig : outputSigs) {
                    if (!Arrays.equals(sig, taskSignature)) {
                        allSigsEquals = false;
                        break;
                    }
                }

                boolean shouldRun = (!allOutputExists || !allSigsEquals) && !completedTasks.contains(task);

                if (!shouldRun) {
                    if (allOutputExists && allSigsEquals)
                    {
                        // Task is successfully completed now or in a previous build.
                        // Only if the conditions in the if-statements are true add the task to the completed set and the
                        // output files to the completed output set
                        completedTasks.add(task);
                        completedOutputs.addAll(task.getOutputs());
                    }
                    continue;
                }

                completedTasks.add(task);

                TaskResult taskResult = new TaskResult(task);
                result.add(taskResult);
                Builder builder = task.getBuilder();
                boolean ok = true;
                int lineNumber = 0;
                String message = null;
                Throwable exception = null;
                boolean abort = false;
                try {
                    builder.build(task);
                    for (IResource r : task.getOutputs()) {
                        state.putSignature(r.getAbsPath(), taskSignature);
                    }

                    for (IResource r : task.getOutputs()) {
                        if (!r.exists()) {
                            message = String.format("Output '%s' not found", r.getAbsPath());
                            ok = false;
                            break;
                        }
                    }
                    completedOutputs.addAll(task.getOutputs());

                } catch (CompileExceptionError e) {
                    ok = false;
                    lineNumber = e.getLineNumber();
                    message = e.getMessage();
                } catch (Throwable e) {
                    ok = false;
                    message = e.getMessage();
                    exception = e;
                    abort = true;

                    // to fix the issue it's easier to see the actual callstack
                    exception.printStackTrace(new java.io.PrintStream(System.out));
                }
                if (!ok) {
                    taskFailed = true;
                    taskResult.setOk(ok);
                    taskResult.setLineNumber(lineNumber);
                    taskResult.setMessage(message);
                    taskResult.setException(exception);
                    // Clear sigs for all outputs when a task fails
                    for (IResource r : task.getOutputs()) {
                        state.putSignature(r.getAbsPath(), new byte[0]);
                    }
                    if (abort) {
                        break run;
                    }
                }
            }
            if (taskFailed) {
                break;
            }
            // set of *all* possible output files
            for (Task<?> task : newTasks) {
                allOutputs.addAll(task.getOutputs());
            }
            tasks.addAll(newTasks);
            newTasks.clear();
        }
        return result;
    }

    /**
     * Set files to compile
     * @param inputs list of input files
     */
    public void setInputs(List<String> inputs) {
        this.inputs = new ArrayList<String>(inputs);
    }

    public HashMap<String, EnumSet<OutputFlags>> getOutputs() {
        return outputs;
    }

    /**
     * Add output flag to resource
     * @param resourcePath output resource absolute path
     * @param flag OutputFlag to add
     */
    public boolean addOutputFlags(String resourcePath, OutputFlags flag) {
        EnumSet<OutputFlags> currentFlags = outputs.get(resourcePath);
        if(currentFlags == null) {
            return false;
        }
        currentFlags.add(flag);
        outputs.replace(resourcePath, currentFlags);
        return true;
    }

    /**
     * Set URLs of libraries to use.
     * @param libUrls list of library URLs
     * @throws IOException
     */
    public void setLibUrls(List<URL> libUrls) throws IOException {
        this.libUrls = libUrls;
    }

    /**
     * Resolve (i.e. download from server) the stored lib URLs.
     * @throws IOException
     */
    public void resolveLibUrls(IProgress progress) throws IOException, LibraryException {
        try {
            String libPath = getLibPath();
            File libDir = new File(libPath);
            // Clean lib dir first
            //FileUtils.deleteQuietly(libDir);
            FileUtils.forceMkdir(libDir);
            // Download libs
            List<File> libFiles = LibraryUtil.convertLibraryUrlsToFiles(libPath, libUrls);
            int count = this.libUrls.size();
            IProgress subProgress = progress.subProgress(count);
            subProgress.beginTask("Download archives", count);
            for (int i = 0; i < count; ++i) {
                BundleHelper.throwIfCanceled(progress);
                File f = libFiles.get(i);
                String sha1 = null;

                if (f.exists()) {
                    ZipFile zipFile = null;

                    try {
                        zipFile = new ZipFile(f);
                        sha1 = zipFile.getComment();
                    } finally {
                        if (zipFile != null) {
                            zipFile.close();
                        }
                    }
                }

                URL url = libUrls.get(i);
                HttpURLConnection connection = (HttpURLConnection) url.openConnection();
                if (sha1 != null) {
                    connection.addRequestProperty("If-None-Match", sha1);
                }

                // Check if URL contains basic auth credentials
                String basicAuthData = null;
                try {
                    URI uri = new URI(url.toString());
                    basicAuthData = uri.getUserInfo();
                } catch (URISyntaxException e1) {
                    // Ignored, could not get URI and basic auth data from URL.
                }

                // Pass correct headers along to server depending on auth alternative.
                if (basicAuthData != null) {
                    String basicAuth = "Basic " + new String(new Base64().encode(basicAuthData.getBytes()));
                    connection.setRequestProperty("Authorization", basicAuth);
                } else {
                    connection.addRequestProperty("X-Email", this.options.get("email"));
                    connection.addRequestProperty("X-Auth", this.options.get("auth"));
                }

                InputStream input = null;
                try {
                    connection.connect();
                    int code = connection.getResponseCode();
                    if (code == 304) {
                        // Reusing cached library
                    } else {
                        boolean serverSha1Match = false;
                        if(code == 200) {
                            // GitHub uses eTags and we can check we have the up to date version by comparing SHA1 and server eTag if we get a 200 OK response
                            if (sha1 != null) {
                                String serverETag = connection.getHeaderField("ETag");
                                if (serverETag != null) {
                                    if (sha1.equals(serverETag.replace("\"", ""))) {
                                        // Reusing cached library
                                        serverSha1Match = true;
                                    }
                                }
                            }
                        }
                        if(!serverSha1Match) {
                            input = new BufferedInputStream(connection.getInputStream());
                            FileUtils.copyInputStreamToFile(input, f);

                            try {
                                ZipFile zip = new ZipFile(f);
                                zip.close();
                            } catch (ZipException e) {
                                f.delete();
                                throw new LibraryException(String.format("The file obtained from %s is not a valid zip file", url.toString()), e);
                            }
                        }
                    }
                    connection.disconnect();
                } catch (ConnectException e) {
                    throw new LibraryException(String.format("Connection refused by the server at %s", url.toString()), e);
                } catch (FileNotFoundException e) {
                    throw new LibraryException(String.format("The URL %s points to a resource which doesn't exist", url.toString()), e);
                } finally {
                    if(input != null) {
                        IOUtils.closeQuietly(input);
                    }
                    subProgress.worked(1);
                }
            }
        }
        catch(IOException ioe) {
            throw ioe;
        }
        catch(LibraryException le) {
            throw le;
        }
        catch(Exception e) {
            throw new LibraryException(e.getMessage(), e);
        }
   }

    /**
     * Set option
     * @param key option key
     * @param value option value
     */
    public void setOption(String key, String value) {
        options.put(key, value);
    }

    /**
     * Get option
     * @param key key to get option for
     * @param defaultValue default value
     * @return mapped value or default value is key doesn't exists
     */
    public String option(String key, String defaultValue) {
        String v = options.get(key);
        if (v != null)
            return v;
        else
            return defaultValue;
    }

    /**
     * Check if an option exists
     * @param key option key to check if it exists
     * @return true if the option exists
     */
    public boolean hasOption(String key) {
        return options.containsKey(key);
    }

    /**
     * Get a map of all options
     * @return A map of options
     */
    public Map<String, String> getOptions() {
        return options;
    }

    class Walker extends FileSystemWalker {

        private Set<String> skipDirs;

        public Walker(Set<String> skipDirs) {
            this.skipDirs = skipDirs;
        }

        @Override
        public void handleFile(String path, Collection<String> results) {
            path = FilenameUtils.normalize(path, true);
            boolean include = true;
            if (skipDirs != null) {
                for (String sd : skipDirs) {
                    if (FilenameUtils.wildcardMatch(path, sd + "/*")) {
                        include = false;
                    }
                }
            }
            // ignore all .files, for instance the .project file that is generated by many Eclipse based editors
            if (FilenameUtils.getBaseName(path).isEmpty()) {
                include = false;
            }
            if (include) {
                String ext = "." + FilenameUtils.getExtension(path);
                Class<? extends Builder<?>> builderClass = extToBuilder.get(ext);
                if (builderClass != null)
                    results.add(path);
            }
        }

        @Override
        public boolean handleDirectory(String path, Collection<String> results) {
            path = FilenameUtils.normalize(path, true);
            if (skipDirs != null) {
                for (String sd : skipDirs) {
                    if (FilenameUtils.equalsNormalized(sd, path)) {
                        return false;
                    }
                    if (FilenameUtils.wildcardMatch(path, sd + "/*")) {
                        return false;
                    }
                }
            }
            return super.handleDirectory(path, results);
        }
    }

    /**
     * Find source files under the root directory
     * @param path path to begin in. Absolute or relative to root-directory
     * @param skipDirs
     * @throws IOException
     */
    public void findSources(String path, Set<String> skipDirs) throws IOException {
        if (new File(path).isAbsolute()) {
            path = normalizeNoEndSeparator(path, true);
            if (path.startsWith(rootDirectory)) {
                path = path.substring(rootDirectory.length());
            } else {
                throw new FileNotFoundException(String.format("the source '%s' must be located under the root '%s'", path, rootDirectory));
            }
        }
        String absolutePath = normalizeNoEndSeparator(FilenameUtils.concat(rootDirectory, path), true);
        if (!new File(absolutePath).exists()) {
            throw new FileNotFoundException(String.format("the path '%s' can not be found under the root '%s'", path, rootDirectory));
        }
        Walker walker = new Walker(skipDirs);
        List<String> results = new ArrayList<String>(1024);
        fileSystem.walk(path, walker, results);
        inputs = results;
    }

    public IResource getResource(String path) {
        return fileSystem.get(FilenameUtils.normalize(path, true));
    }

    public IResource getGameProjectResource() {
        return getResource("/game.project");
    }

    public static String stripLeadingSlash(String path) {
        while (path.length() > 0 && path.charAt(0) == '/') {
            path = path.substring(1);
        }
        return path;
    }

    public void findResourcePaths(String _path, Collection<String> result) {
        final String path = Project.stripLeadingSlash(_path);
        fileSystem.walk(path, new FileSystemWalker() {
            public void handleFile(String path, Collection<String> results) {
                results.add(FilenameUtils.normalize(path, true));
            }
        }, result);
    }

    // Finds the first level of directories in a path
    public void findResourceDirs(String _path, Collection<String> result) {
        // Make sure the path has Unix separators, since this is how
        // paths are specified game project relative internally.
        final String path = Project.stripLeadingSlash(FilenameUtils.separatorsToUnix(_path));
        fileSystem.walk(path, new FileSystemWalker() {
            public boolean handleDirectory(String dir, Collection<String> results) {
                if (path.equals(dir)) {
                    return true;
                }
                results.add(FilenameUtils.getName(FilenameUtils.normalizeNoEndSeparator(dir)));
                return false; // skip recursion
            }
            public void handleFile(String path, Collection<String> results) { // skip any files
            }
        }, result);
    }

    public List<Task<?>> getTasks() {
        return Collections.unmodifiableList(this.newTasks);
    }

    public TextureProfiles getTextureProfiles() {
        return textureProfiles;
    }

    public void setTextureProfiles(TextureProfiles textureProfiles) {
        this.textureProfiles = textureProfiles;
    }

    public void excludeCollectionProxy(String path) {
        this.excludedCollectionProxies.add(path);
    }

    public final List<String> getExcludedCollectionProxies() {
        return this.excludedCollectionProxies;
    }

}
