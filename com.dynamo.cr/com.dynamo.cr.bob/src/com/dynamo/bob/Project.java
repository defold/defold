package com.dynamo.bob;

import static org.apache.commons.io.FilenameUtils.normalizeNoEndSeparator;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
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
import java.util.EnumSet;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.zip.ZipException;
import java.util.zip.ZipFile;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.io.filefilter.DirectoryFileFilter;
import org.apache.commons.io.filefilter.RegexFileFilter;
import org.apache.commons.lang.StringUtils;
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
import com.dynamo.bob.bundle.OSX32Bundler;
import com.dynamo.bob.bundle.OSX64Bundler;
import com.dynamo.bob.bundle.Win32Bundler;
import com.dynamo.bob.bundle.Win64Bundler;
import com.dynamo.bob.fs.ClassLoaderMountPoint;
import com.dynamo.bob.fs.FileSystemWalker;
import com.dynamo.bob.fs.IFileSystem;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ZipMountPoint;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.pipeline.ExtenderUtil.JavaRExtenderResource;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.LibraryUtil;
import com.dynamo.bob.util.ReportGenerator;
import com.dynamo.bob.util.Exec.Result;
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

    private BobProjectProperties projectProperties;
    private Publisher publisher;

    private TextureProfiles textureProfiles;

    public Project(IFileSystem fileSystem) {
        this.fileSystem = fileSystem;
        this.fileSystem.setRootDirectory(rootDirectory);
        this.fileSystem.setBuildDirectory(buildDirectory);
    }

    public Project(IFileSystem fileSystem, String sourceRootDirectory, String buildDirectory) {
        this.rootDirectory = normalizeNoEndSeparator(new File(sourceRootDirectory).getAbsolutePath(), true);
        this.buildDirectory = normalizeNoEndSeparator(buildDirectory, true);
        this.fileSystem = fileSystem;
        this.fileSystem.setRootDirectory(this.rootDirectory);
        this.fileSystem.setBuildDirectory(this.buildDirectory);
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

    @SuppressWarnings("unchecked")
    private void doScan(Set<String> classNames) {
        for (String className : classNames) {
            // Ignore TexcLibrary to avoid it being loaded and initialized
            if (!className.startsWith("com.dynamo.bob.TexcLibrary")) {
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
        if (shouldPublish) {
            try {
                IResource publisherSettings = this.fileSystem.get("/liveupdate.settings");
                if (publisherSettings.exists()) {
                    ByteArrayInputStream is = new ByteArrayInputStream(publisherSettings.getContent());
                    PublisherSettings settings = PublisherSettings.load(is);

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
                    throw new CompileExceptionError("There is no liveupdate.settings file", null);
                }
            } catch (Throwable e) {
                throw new CompileExceptionError(null, 0, e.getMessage(), e);
            }
        } else {
            this.publisher = new NullPublisher(new PublisherSettings());
        }
    }

    public void clearProjectProperties() {
        projectProperties = new BobProjectProperties();
    }

    public void loadProjectFile() throws IOException, ParseException {
        clearProjectProperties();
        IResource gameProject = this.fileSystem.get("/game.project");
        if (gameProject.exists()) {
            ByteArrayInputStream is = new ByteArrayInputStream(gameProject.getContent());
            projectProperties.load(is);
        } else {
            logWarning("No game.project found");
        }
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
            // Pass on unmodified
            throw e;
        } catch (MultipleCompileException e) {
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
        bundlers = new HashMap<Platform, Class<? extends IBundler>>();
        bundlers.put(Platform.X86Darwin, OSX32Bundler.class);
        bundlers.put(Platform.X86_64Darwin, OSX64Bundler.class);
        bundlers.put(Platform.X86Linux, LinuxBundler.class);
        bundlers.put(Platform.X86_64Linux, LinuxBundler.class);
        bundlers.put(Platform.X86Win32, Win32Bundler.class);
        bundlers.put(Platform.X86_64Win32, Win64Bundler.class);
        bundlers.put(Platform.Armv7Android, AndroidBundler.class);
        bundlers.put(Platform.Armv7Darwin, IOSBundler.class);
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
        bundleDir.mkdirs();
        bundler.bundleApplication(this, bundleDir);
        m.worked(1);
        m.done();
    }

    static boolean anyFailing(Collection<TaskResult> results) {
        for (TaskResult taskResult : results) {
            if (!taskResult.isOk()) {
                return true;
            }
        }

        return false;
    }

    private void generateRJava(List<String> resourceDirectories, List<String> extraPackages, File manifestFile, File outputDirectory) throws CompileExceptionError {

        try {
            // Include built-in/default facebook and gms resources
            resourceDirectories.add(Bob.getPath("res/facebook"));
            resourceDirectories.add(Bob.getPath("res/google-play-services"));
            extraPackages.add("com.facebook");
            extraPackages.add("com.google.android.gms");

            Map<String, String> aaptEnv = new HashMap<String, String>();
            if (Platform.getHostPlatform() == Platform.X86_64Linux || Platform.getHostPlatform() == Platform.X86Linux) {
                aaptEnv.put("LD_LIBRARY_PATH", Bob.getPath(String.format("%s/lib", Platform.getHostPlatform().getPair())));
            }

            // Run aapt to generate R.java files
            List<String> args = new ArrayList<String>();
            args.add(Bob.getExe(Platform.getHostPlatform(), "aapt"));
            args.add("package");
            args.add("--no-crunch");
            args.add("-f");
            args.add("--extra-packages");
            args.add(StringUtils.join(extraPackages, ":"));
            args.add("-m");
            args.add("--auto-add-overlay");
            args.add("-M"); args.add(manifestFile.getAbsolutePath());
            args.add("-I"); args.add(Bob.getPath("lib/android.jar"));
            args.add("-J"); args.add(outputDirectory.getAbsolutePath());

            for( String s : resourceDirectories )
            {
                args.add("-S"); args.add(s);
            }

            Result res = Exec.execResultWithEnvironment(aaptEnv, args);

            if (res.ret != 0) {
                String msg = new String(res.stdOutErr);
                throw new CompileExceptionError(null, -1, "Failed building Android resources to R.java: " + msg);
            }

        } catch (Exception e) {
            throw new CompileExceptionError(null, -1, "Failed building Android resources to R.java: " + e.getMessage());
        }
    }

    public void buildEngine(IProgress monitor) throws IOException, CompileExceptionError, MultipleCompileException {
        String pair = option("platform", null);
        Platform p = Platform.getHostPlatform();
        if (pair != null) {
            p = Platform.get(pair);
        }

        if (p == null) {
            throw new CompileExceptionError(null, -1, String.format("Platform %s not supported", pair));
        }
        PlatformArchitectures platformArchs = p.getArchitectures();

        // Store the engine one level above the content build since that folder gets removed during a distclean
        String internalDir = FilenameUtils.concat(rootDirectory, ".internal");
        File cacheDir = new File(FilenameUtils.concat(internalDir, "cache"));
        cacheDir.mkdirs();

        String serverURL = this.option("build-server", "https://build.defold.com");

        // Get SHA1 and create log file
        String sdkVersion = this.option("defoldsdk", EngineVersion.sha1);
        File logFile = File.createTempFile("build_" + sdkVersion + "_", ".txt");
        logFile.deleteOnExit();

        String[] platformStrings = platformArchs.getArchitectures();
        IProgress m = monitor.subProgress(platformStrings.length);
        m.beginTask("Building engine...", 0);

        // Build all skews of platform
        String outputDir = options.getOrDefault("binary-output", FilenameUtils.concat(rootDirectory, "build"));
        for (int i = 0; i < platformStrings.length; ++i) {
            Platform platform = Platform.get(platformStrings[i]);

            String buildPlatform = platform.getExtenderPair();
            File buildDir = new File(FilenameUtils.concat(outputDir, buildPlatform));
            buildDir.mkdirs();

            String defaultName = platform.formatBinaryName("dmengine");
            File exe = new File(FilenameUtils.concat(buildDir.getAbsolutePath(), defaultName));
            List<ExtenderResource> allSource = ExtenderUtil.getExtensionSources(this, platform);


            File classesDexFile = null;
            if (platform.equals(Platform.Armv7Android)) {

                // If we are building for Android, we expect a classes.dex file to be returned as well.
                classesDexFile = new File(FilenameUtils.concat(buildDir.getAbsolutePath(), "classes.dex"));

                List<String> resDirs = new ArrayList<>();
                List<String> extraPackages = new ArrayList<>();

                // Create temp files and directories needed to run aapt and output R.java files
                File tmpDir = File.createTempFile("tmp", "bundle");
                tmpDir.delete();
                tmpDir.mkdir();
                tmpDir.deleteOnExit();

                // <tmpDir>/resources - Where to collect all resources needed for aapt
                File resOutput = new File(tmpDir, "resources");
                resOutput.delete();
                resOutput.mkdir();
                resOutput.deleteOnExit();

                // <tmpDir>/rjava - Output directory of aapt, all R.java files will be stored here
                File javaROutput = new File(tmpDir, "rjava");
                javaROutput.delete();
                javaROutput.mkdir();
                javaROutput.deleteOnExit();

                // Get all Android specific resources needed to create R.java files
                Map<String, IResource> resources = ExtenderUtil.getAndroidResource(this);
                ExtenderUtil.writeResourcesToDirectory(resources, resOutput);
                resDirs.add(resOutput.getAbsolutePath());

                // Generate AndroidManifest.xml and output icons resources
                File manifestFile = new File(tmpDir, "AndroidManifest.xml");
                manifestFile.deleteOnExit();
                BundleHelper helper = new BundleHelper(this, Platform.Armv7Android, tmpDir, "");
                helper.createAndroidManifest(getProjectProperties(), getRootDirectory(), manifestFile, resOutput, "dummy");

                // Run aapt to generate R.java files
                generateRJava(resDirs, extraPackages, manifestFile, javaROutput);

                // Collect all *.java files from aapt output
                Collection<File> javaFiles = FileUtils.listFiles(
                        javaROutput,
                        new RegexFileFilter(".+\\.java"),
                        DirectoryFileFilter.DIRECTORY
                );

                // Add outputs as _app/rjava/ sources
                String rootRJavaPath = "_app/rjava/";
                for (File javaFile : javaFiles) {
                    String relative = javaROutput.toURI().relativize(javaFile.toURI()).getPath();
                    String outputPath = rootRJavaPath + relative;
                    allSource.add(new JavaRExtenderResource(javaFile, outputPath));
                }
            }

            ExtenderClient extender = new ExtenderClient(serverURL, cacheDir);
            BundleHelper.buildEngineRemote(extender, buildPlatform, sdkVersion, allSource, logFile, defaultName, exe, classesDexFile);

            m.worked(1);
        }

        m.done();
    }

    private void cleanEngine(IProgress monitor) throws IOException, CompileExceptionError {
        String pair = option("platform", null);
        Platform p = Platform.getHostPlatform();
        if (pair != null) {
            p = Platform.get(pair);
        }

        if (p == null) {
            throw new CompileExceptionError(null, -1, String.format("Platform %s not supported", pair));
        }
        PlatformArchitectures platformArchs = p.getArchitectures();

        String[] platformStrings = platformArchs.getArchitectures();
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

            String defaultName = platform.formatBinaryName("dmengine");
            File exe = new File(FilenameUtils.concat(buildDir.getAbsolutePath(), defaultName));
            if (exe.exists()) {
                exe.delete();
            }

            // If we are building for Android, we expect a classes.dex file to be returned as well.
            if (platform.equals(Platform.Armv7Android)) {
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

        monitor.beginTask("", 100);

        for (String command : commands) {
            if (command.equals("build")) {

                // Do early test if report files are writeable before we start building
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
                m.beginTask("Building...", newTasks.size());
                result = runTasks(m);
                m.done();
                if (anyFailing(result)) {
                    break;
                }

                // Get or build engine binary
                boolean hasNativeExtensions = ExtenderUtil.hasNativeExtensions(this);
                if (hasNativeExtensions) {
                    buildEngine(monitor);
                } else {
                    // Remove the remote built executables in the build folder, they're still in the cache
                    cleanEngine(monitor);
                }

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

            } else if (command.equals("clean")) {
                IProgress m = monitor.subProgress(1);
                m.beginTask("Cleaning...", newTasks.size());
                for (Task<?> t : newTasks) {
                    List<IResource> outputs = t.getOutputs();
                    for (IResource r : outputs) {
                        r.remove();
                        m.worked(1);
                    }
                }
                m.done();
            } else if (command.equals("distclean")) {
                IProgress m = monitor.subProgress(1);
                m.beginTask("Cleaning...", newTasks.size());
                FileUtils.deleteDirectory(new File(FilenameUtils.concat(rootDirectory, buildDirectory)));
                m.worked(1);
                m.done();
            } else if (command.equals("bundle")) {
                bundle(monitor);
            }
        }

        monitor.done();
        state.save(stateResource);
        fileSystem.saveCache();
        return result;
    }

    @SuppressWarnings({ "rawtypes", "unchecked" })
    List<TaskResult> runTasks(IProgress monitor) throws IOException {

        // set of all completed tasks. The set includes both task run
        // in this session and task already completed (output already exists with correct signatures, see below)
        // the set also contains failed tasks
        Set<Task> completedTasks = new HashSet<Task>();

        // the set of all output files generated
        // in this or previous session
        Set<IResource> completedOutputs = new HashSet<IResource>();

        List<TaskResult> result = new ArrayList<TaskResult>();

        List<Task<?>> tasks = new ArrayList<Task<?>>(newTasks);
        // set of *all* possible output files
        Set<IResource> allOutputs = new HashSet<IResource>();
        for (Task<?> task : newTasks) {
            allOutputs.addAll(task.getOutputs());
        }
        newTasks.clear();

        // Keep track of the paths for all outputs
        outputs = new HashMap<String, EnumSet<OutputFlags>>(allOutputs.size());
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
                // deps are the task input files generated by another task not yet completed,
                // i.e. "solve" the dependency graph
                Set<IResource> deps = new HashSet<IResource>();
                deps.addAll(task.getInputs());
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
            if (progress.isCanceled()) {
                break;
            }
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
                        if(sha1 != null)
                        {
                            String serverETag = connection.getHeaderField("ETag");
                            if(serverETag != null)
                            {
                                if(sha1.equals(serverETag.replace("\"", ""))) {
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
        return fileSystem.get(path);
    }

    public void findResourcePaths(String path, Collection<String> result)
    {
        fileSystem.walk(path, new FileSystemWalker() {
            public void handleFile(String path, Collection<String> results) {
                results.add(path);
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
