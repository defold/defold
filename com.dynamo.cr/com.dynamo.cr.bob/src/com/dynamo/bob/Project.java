// Copyright 2020-2025 The Defold Foundation
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

package com.dynamo.bob;

import static org.apache.commons.io.FilenameUtils.normalizeNoEndSeparator;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileInputStream;
import java.io.FileOutputStream;
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
import java.nio.file.attribute.FileTime;
import java.text.ParseException;
import java.util.*;
import java.util.concurrent.Executors;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Callable;
import java.util.concurrent.Future;
import java.util.concurrent.ExecutionException;
import java.util.jar.Attributes;
import java.util.jar.JarFile;
import java.util.jar.Manifest;
import java.util.logging.Level;
import java.util.stream.Collectors;
import java.util.zip.ZipException;
import java.util.zip.ZipFile;
import java.util.zip.ZipOutputStream;

import com.defold.extension.pipeline.ILuaTranspiler;
import com.defold.extension.pipeline.texture.TextureCompressorPreset;
import com.dynamo.bob.archive.ArchiveBuilder;
import com.dynamo.bob.archive.publisher.FolderPublisher;
import com.dynamo.bob.fs.ClassLoaderMountPoint;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.fs.DefaultResource;
import com.dynamo.bob.fs.FileSystemMountPoint;
import com.dynamo.bob.fs.FileSystemWalker;
import com.dynamo.bob.fs.IFileSystem;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ZipMountPoint;
import com.dynamo.bob.plugin.PluginScanner;
import com.dynamo.bob.util.BuildInputDataCollector;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.codec.binary.Base64;

import com.defold.extender.client.ExtenderClient;
import com.defold.extender.client.ExtenderClientException;
import com.defold.extender.client.ExtenderResource;

import com.dynamo.bob.archive.EngineVersion;
import com.dynamo.bob.archive.publisher.AWSPublisher;
import com.dynamo.bob.archive.publisher.NullPublisher;
import com.dynamo.bob.archive.publisher.Publisher;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.archive.publisher.ZipPublisher;

import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.bundle.IBundler;
import com.dynamo.bob.bundle.BundlerParams;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.pipeline.IShaderCompiler;
import com.dynamo.bob.pipeline.ShaderCompilers;
import com.dynamo.bob.pipeline.TextureGenerator;
import com.defold.extension.pipeline.texture.TextureCompression;
import com.defold.extension.pipeline.texture.ITextureCompressor;
import com.dynamo.bob.plugin.IPlugin;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.LibraryUtil;
import com.dynamo.bob.util.ReportGenerator;
import com.dynamo.bob.util.TimeProfiler;
import com.dynamo.bob.util.StringUtil;
import com.dynamo.graphics.proto.Graphics.TextureProfiles;

import com.dynamo.bob.cache.ResourceCache;
import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;

/**
 * Project abstraction. Contains input files, builder, tasks, etc
 * @author Christian Murray
 *
 */
public class Project {

    private static Logger logger = Logger.getLogger(Project.class.getName());

    public final static String LIB_DIR = ".internal/lib";
    public final static String CACHE_DIR = ".internal/cache";
    public final static String PLUGINS_DIR = "./build/plugins";
    private static ClassLoaderScanner scanner = null;

    public enum OutputFlags {
        NONE,
        UNCOMPRESSED,
        ENCRYPTED
    }

    private ExecutorService executor = Executors.newCachedThreadPool();
    private ResourceCache resourceCache = new ResourceCache();
    private IFileSystem fileSystem;
    private Map<String, Class<? extends Builder>> extToBuilder = new HashMap<String, Class<? extends Builder>>();
    private Map<String, String> inextToOutext = new HashMap<>();
    private List<String> inputs = new ArrayList<String>();
    private HashMap<String, EnumSet<OutputFlags>> outputs = new HashMap<String, EnumSet<OutputFlags>>();
    private HashMap<String, Task> tasks;
    private Set<String> circularDependencyChecker = new LinkedHashSet<>();
    private State state;
    private String rootDirectory = ".";
    private String buildDirectory = "build";
    private Map<String, String> options = new HashMap<String, String>();
    private List<URL> libUrls = new ArrayList<URL>();
    private List<String> propertyFiles = new ArrayList<>();
    private List<String> buildServerHeaders = new ArrayList<>();
    private List<String> engineBuildDirs = new ArrayList<>();
    private List<String> allResourcePathsCache; // Cache for all resource paths, since Bob doesn't change project files during build
    private BobProjectProperties projectProperties;
    private Publisher publisher;
    private Map<String, Map<Long, IResource>> hashToResource = new HashMap<>();

    private TextureProfiles textureProfiles;
    private List<Class<? extends IBundler>> bundlerClasses = new ArrayList<>();
    private Set<Class<? extends IPlugin>> pluginClasses = new HashSet<>();
    private ClassLoader classLoader = null;

    private List<Class<? extends IShaderCompiler>> shaderCompilerClasses = new ArrayList();
    private List<Class<? extends ITextureCompressor>> textureCompressorClasses = new ArrayList();

    private ArchiveBuilder archiveBuilder;

    public void setArchiveBuilder(ArchiveBuilder archiveBuilder) {
        this.archiveBuilder = archiveBuilder;
    }

    public ArchiveBuilder getArchiveBuilder() {
        return this.archiveBuilder;
    }

    public Project(IFileSystem fileSystem) {
        this.fileSystem = fileSystem;
        this.fileSystem.setRootDirectory(rootDirectory);
        this.fileSystem.setBuildDirectory(buildDirectory);
        this.allResourcePathsCache = null;
        clearProjectProperties();
    }

    public Project(IFileSystem fileSystem, String sourceRootDirectory, String buildDirectory) {
        this.rootDirectory = normalizeNoEndSeparator(new File(sourceRootDirectory).getAbsolutePath(), true);
        this.buildDirectory = normalizeNoEndSeparator(buildDirectory, true);
        this.fileSystem = fileSystem;
        this.fileSystem.setRootDirectory(this.rootDirectory);
        this.fileSystem.setBuildDirectory(this.buildDirectory);
        this.allResourcePathsCache = null;
        clearProjectProperties();
    }

    // For the editor
    public Project(ClassLoader loader, IFileSystem fileSystem, String sourceRootDirectory, String buildDirectory) {
        this.classLoader = loader;
        this.rootDirectory = normalizeNoEndSeparator(new File(sourceRootDirectory).getAbsolutePath(), true);
        this.buildDirectory = normalizeNoEndSeparator(buildDirectory, true);
        this.fileSystem = fileSystem;
        this.fileSystem.setRootDirectory(this.rootDirectory);
        this.fileSystem.setBuildDirectory(this.buildDirectory);
        this.allResourcePathsCache = null;
        clearProjectProperties();
    }

    // For tests
    public void cleanupResourcePathsCache() {
        allResourcePathsCache = null;
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

    public String getPluginsDirectory() {
        return FilenameUtils.concat(rootDirectory, PLUGINS_DIR);
    }

    public String getBinaryOutputDirectory() {
        return options.getOrDefault("binary-output", FilenameUtils.concat(rootDirectory, "build"));
    }

    public String getLibPath() {
        return FilenameUtils.concat(rootDirectory, LIB_DIR);
    }

    public String getBuildCachePath() {
        return FilenameUtils.concat(rootDirectory, CACHE_DIR);
    }

    public String getSystemEnv(String name) {
        return System.getenv(name);
    }

    public String getSystemProperty(String name) {
        return System.getProperty(name);
    }

    public String getLocalResourceCacheDirectory() {
        return option("resource-cache-local", null);
    }

    public String getRemoteResourceCacheDirectory() {
        return option("resource-cache-remote", null);
    }

    public String getRemoteResourceCacheUser() {
        return option("resource-cache-remote-user", getSystemEnv("DM_BOB_RESOURCE_CACHE_REMOTE_USER"));
    }

    public String getRemoteResourceCachePass() {
        return option("resource-cache-remote-pass", getSystemEnv("DM_BOB_RESOURCE_CACHE_REMOTE_PASS"));
    }

    public int getMaxCpuThreads() {
        String maxThreadsOpt = option("max-cpu-threads", null);
        if (maxThreadsOpt == null) {
            return getDefaultMaxCpuThreads();
        }
        int threads = Integer.parseInt(maxThreadsOpt);
        if (threads <= 0) {
            threads = java.lang.Math.max(1, Runtime.getRuntime().availableProcessors() + threads);
        }
        return threads;
    }

    /**
     * Returns half of the threads specified by the user, but no more than half of the available threads.
     * @return half of specified or available threads
     */
    public int getHalfThreads() {
        int halfOfAvailableThreads = Runtime.getRuntime().availableProcessors() / 2;
        return Math.max(Math.min(halfOfAvailableThreads, getMaxCpuThreads() / 2), 1);
    }

    public BobProjectProperties getProjectProperties() {
        return projectProperties;
    }

    /**
     * Convert an absolute path to a path relative to the project root
     * @param path The path to relativize
     * @return Relative path
     */
    public String getPathRelativeToRootDirectory(String path) {
        return Path.of(rootDirectory).relativize(Path.of(path)).toString();
    }

    public void setPublisher(Publisher publisher) {
        this.publisher = publisher;
    }

    public Publisher getPublisher() {
        return this.publisher;
    }

    private ClassLoaderScanner createClassLoaderScanner() throws IOException {
        scanner = new ClassLoaderScanner(getClassLoader());
        return scanner;
    }

    public ClassLoader getClassLoader() {
        if (classLoader == null)
            classLoader = this.getClass().getClassLoader();
        return classLoader;
    }

    public static IClassScanner getClassLoaderScanner() {
        return scanner;
    }

    public static Class<?> getClass(String className) {
        try {
            return Class.forName(className, true, scanner.getClassLoader());
        } catch(ClassNotFoundException e) {
            return null;
        }
    }

    /**
     * Scan package for builder classes
     * @param scanner class scanner
     * @param pkg package name to be scanned
     */
    public void scan(IClassScanner scanner, String pkg) {
        TimeProfiler.start("scan %s", pkg);
        Set<String> classNames = scanner.scan(pkg);
        doScan(scanner, classNames);
        TimeProfiler.stop();
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
    private void doScan(IClassScanner scanner, Set<String> classNames) {
        TimeProfiler.start("doScan");
        boolean is_bob_light = getManifestInfo("is-bob-light") != null;
        List<String> filteredClassNames = classNames.stream()
                .filter(className ->
                        // classes with static initializers we don't want to initialize on this stage
                        !className.startsWith("com.dynamo.bob.pipeline.TexcLibrary") &&
                        !className.startsWith("com.dynamo.bob.pipeline.Shaderc") &&
                        !className.startsWith("com.dynamo.bob.pipeline.ModelImporter") &&
                        // namespaces we don't need to scan
                        !className.startsWith("com.dynamo.bob.pipeline.antlr") &&
                        // classes we don't need to bob light
                        !(is_bob_light && className.startsWith("com.dynamo.bob.archive.publisher.AWSPublisher")) &&
                        !(is_bob_light && className.startsWith("com.dynamo.bob.pipeline.ExtenderUtil")) &&
                        !(is_bob_light && className.startsWith("com.dynamo.bob.bundle.BundleHelper")))
                .collect(Collectors.toList());
        for (String className : filteredClassNames) {
            try {
                TimeProfiler.start(className);
                Class<?> klass = Class.forName(className, true, scanner.getClassLoader());
                BuilderParams builderParams = klass.getAnnotation(BuilderParams.class);
                if (builderParams != null) {
                    for (String inExt : builderParams.inExts()) {
                        extToBuilder.put(inExt, (Class<? extends Builder>) klass);
                        inextToOutext.put(inExt, builderParams.outExt());
                    }
                    Builder.addParamsDigest(klass, this.getOptions(), builderParams);
                    ProtoParams protoParams = klass.getAnnotation(ProtoParams.class);
                    if (protoParams != null) {
                        ProtoBuilder.addMessageClass(builderParams.outExt(), protoParams.messageClass());
                        ProtoBuilder.addProtoDigest(protoParams.messageClass());
                        for (String ext : builderParams.inExts()) {
                            Class<?> inputClass = protoParams.srcClass();
                            if (inputClass != null) {
                                ProtoBuilder.addMessageClass(ext, protoParams.srcClass());
                            }
                        }
                    }
                }

                if (IBundler.class.isAssignableFrom(klass)) {
                    if (!klass.equals(IBundler.class)) {
                        bundlerClasses.add((Class<? extends IBundler>) klass);
                    }
                }

                if (IShaderCompiler.class.isAssignableFrom(klass)) {
                    if (!klass.equals(IShaderCompiler.class)) {
                        shaderCompilerClasses.add((Class<? extends IShaderCompiler>) klass);
                    }
                }

                if (ITextureCompressor.class.isAssignableFrom(klass)) {
                    if (!klass.equals(ITextureCompressor.class)) {
                        textureCompressorClasses.add((Class<? extends ITextureCompressor>) klass);
                    }
                }

                if (IPlugin.class.isAssignableFrom(klass)) {
                    if (!klass.equals(IPlugin.class)) {
                        pluginClasses.add((Class<? extends IPlugin>) klass);
                    }
                }
                TimeProfiler.stop();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
        TimeProfiler.stop(); // Final stop after plugin registration
    }

    static String[][] extensionMapping = new String[][] {
        {".camera", ".camerac"},
        {".buffer", ".bufferc"},
        {".mesh", ".meshc"},
        {".collectionproxy", ".collectionproxyc"},
        {".collisionobject", ".collisionobjectc"},
        {".particlefx", ".particlefxc"},
        {".gui", ".guic"},
        {".model", ".modelc"},
        {".script", ".scriptc"},
        {".sound", ".soundc"},
        {".wav", ".soundc"},
        {".ogg", ".soundc"},
        {".opus", ".soundc"},
        {".collectionfactory", ".collectionfactoryc"},
        {".factory", ".factoryc"},
        {".light", ".lightc"},
        {".label", ".labelc"},
        {".sprite", ".spritec"},
        {".tilegrid", ".tilemapc"},
        {".tilemap", ".tilemapc"},
    };

    private String generateCircularDependencyErrorMessage(String dependency) {
        StringBuilder errorMessage = new StringBuilder("\nCircular dependency detected:\n");

        for (String element : circularDependencyChecker) {
            if (element.equals(dependency)) {
                errorMessage.append("-> ").append(element).append(" (Circular Point)\n");
            } else {
                errorMessage.append(element).append("\n");
            }
        }
        errorMessage.append("-> ").append(dependency).append(" (Circular Point)");

        return errorMessage.toString();
    }

    public String replaceExt(String inExt) {
        for (int i = 0; i < extensionMapping.length; i++) {
            if (extensionMapping[i][0].equals(inExt))
            {
                return extensionMapping[i][1];
            }
        }
        String outExt = inextToOutext.get(inExt); // Get the output ext, or use the inExt as default
        if (outExt != null)
            return outExt;
        return inExt;
    }

    private Class<? extends Builder> getBuilderFromExtension(String input) {
        String ext = "." + FilenameUtils.getExtension(input);
        Class<? extends Builder> builderClass = extToBuilder.get(ext);
        return builderClass;
    }

    /**
     * Returns builder class for resource
     * @param input input resource
     * @return class
     */
    public Class<? extends Builder> getBuilderFromExtension(IResource input) {
        return getBuilderFromExtension(input.getPath());
    }

    /**
     * Create task from resource. Typically called from builder
     * that create intermediate output/input-files
     * @param inputResource input resource
     * @return task
     * @throws CompileExceptionError
     */
    public Task createTask(IResource inputResource) throws CompileExceptionError {
        Class<? extends Builder> builderClass = getBuilderFromExtension(inputResource);
        if (builderClass == null) {
            logWarning("No builder for '%s' found", inputResource);
            return null;
        }

        return createTask(inputResource, builderClass);
    }

    /**
     * Create task from resource with explicit builder.
     * Make sure that task is unique.
     * @param inputResource input resource
     * @param builderClass class to build resource with
     * @return task
     * @throws CompileExceptionError
     */
    public Task createTask(IResource inputResource, Class<? extends Builder> builderClass) throws CompileExceptionError {
        // It's possible to build the same resource using different builders
        String key = inputResource.getPath()+" "+builderClass;
        if (!circularDependencyChecker.add(key)) {
            throw new CompileExceptionError(generateCircularDependencyErrorMessage(key), null);
        }
        Task task = tasks.get(key);
        if (task != null) {
            circularDependencyChecker.remove(key);
            return task;
        }
        TimeProfiler.start();
        TimeProfiler.addData("type", "createTask");
        Builder builder;
        try {
            builder = builderClass.newInstance();
            builder.setProject(this);
            task = builder.create(inputResource);
            if (task != null) {
                TimeProfiler.addData("output", StringUtil.truncate(task.getOutputsString(), 1000));
                TimeProfiler.addData("name", task.getName());
                tasks.put(key, task);
            }
            circularDependencyChecker.remove(key);
            return task;
        } catch (CompileExceptionError e) {
            // Just pass CompileExceptionError on unmodified
            throw e;
        } catch (Exception e) {
            throw new RuntimeException(e);
        } finally {
            TimeProfiler.stop();
        }
    }

    private void createTasks() throws CompileExceptionError {
        circularDependencyChecker = new LinkedHashSet<>();
        tasks = new HashMap<String, Task>();
        if(this.inputs == null || this.inputs.isEmpty()) {
            createTask(getGameProjectResource());
        }
        else {
            for (String input : this.inputs) {
                createTask(getResource(input));
            }
        }
    }

    private void logWarning(String fmt, Object... args) {
        System.err.println(String.format(fmt, args));
    }
    private void logInfo(String fmt, Object... args) {
        System.out.println(String.format(fmt, args));
    }

    public void createPublisher() throws CompileExceptionError {
        boolean shouldPublish = this.option("liveupdate", "false").equals("true");
        try {
            String settingsPath = this.getProjectProperties().getStringValue("liveupdate", "settings", "/liveupdate.settings"); // if no value set use old hardcoded path (backward compatability)
            IResource publisherSettings = this.fileSystem.get(settingsPath);
            if (!publisherSettings.exists()) {
                if (shouldPublish) {
                    IResource gameProject = getGameProjectResource();
                    throw new CompileExceptionError(gameProject, 0, "There is no liveupdate.settings file specified in game.project or the file is missing from disk.");
                } else {
                    this.publisher = new NullPublisher(new PublisherSettings());
                }
            } else {
                PublisherSettings settings = PublisherSettings.load(publisherSettings);
                if (shouldPublish) {
                    if (PublisherSettings.PublishMode.Amazon.equals(settings.getMode())) {
                        this.publisher = new AWSPublisher(settings);
                    } else if (PublisherSettings.PublishMode.Zip.equals(settings.getMode())) {
                        this.publisher = new ZipPublisher(getRootDirectory(), settings);
                    } else if (PublisherSettings.PublishMode.Folder.equals(settings.getMode())) {
                        this.publisher = new FolderPublisher(getRootDirectory(), settings);
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

    private static void loadPropertiesData(BobProjectProperties properties, byte[] data, Boolean isMeta, String filepath) throws IOException {
        ByteArrayInputStream is = new ByteArrayInputStream(data);
        try {
            properties.load(is, isMeta);
        } catch(ParseException e) {
            throw new IOException("Could not parse: " + filepath);
        }
    }

    private static void loadPropertiesFile(BobProjectProperties properties, String filepath, Boolean isMeta) throws IOException {
        Path pathHandle = Paths.get(filepath);
        if (!Files.exists(pathHandle) || !pathHandle.toFile().isFile())
            throw new IOException(filepath + " is not a file");
        loadPropertiesData(properties, Files.readAllBytes(pathHandle), isMeta, filepath);
    }

    // Loads the properties from a game project settings file
    // Also adds any properties specified with the "--settings" flag
    public static BobProjectProperties loadProperties(Project project, IResource projectFile, List<String> settingsFiles, boolean scanExtensions) throws IOException {
        if (!projectFile.exists()) {
            throw new IOException(String.format("Project file not found: %s", projectFile.getAbsPath()));
        }

        BobProjectProperties properties = new BobProjectProperties();
        try {
            // load meta.properties embeded in bob.jar
            properties.loadDefaultMetaFile();
            properties.cleanupEmptyProperties();
            if (scanExtensions) {
                // load property files from extensions
                List<String> extensionFolders = ExtenderUtil.getExtensionFolders(project);
                if (!extensionFolders.isEmpty()) {
                    for (String extension : extensionFolders) {
                        IResource resource = project.getResource(extension + "/" + BobProjectProperties.PROPERTIES_EXTENSION_FILE);
                        if (resource.exists()) {
                            // resources from extensions in ZIP files can't be read as files, but getContent() works fine
                            loadPropertiesData(properties, resource.getContent(), true, resource.getPath());
                        }
                    }
                }
            }
            // load property file from the project
            IResource gameProjectProperties = projectFile.getResource(BobProjectProperties.PROPERTIES_PROJECT_FILE);
            if (gameProjectProperties.exists()) {
               loadPropertiesFile(properties, gameProjectProperties.getAbsPath(), true);
            }
            // load game.project file
            Project.loadPropertiesFile(properties, projectFile.getAbsPath(), false);
        } catch(ParseException e) {
            throw new IOException("Could not parse: " + projectFile.getAbsPath());
        }
        // load settings file specified in `--settings` for bob.jar
        for (String filepath : settingsFiles) {
            Project.loadPropertiesFile(properties, filepath, false);
        }

        return properties;
    }

    public void loadProjectFile(boolean scanExtensions) throws IOException {
        IResource gameProject = getGameProjectResource();
        if (gameProject.exists()) {
            projectProperties = Project.loadProperties(this, gameProject, this.getPropertyFiles(), scanExtensions);
        }
    }

    // External API. See https://github.com/defold/extension-prometheus
    public void loadProjectFile() throws IOException {
        loadProjectFile(true);
    }

    public void addBuildServerHeader(String header) {
        buildServerHeaders.add(header);
    }

    public void addPropertyFile(String propertyFile) {
        propertyFiles.add(propertyFile);
    }

    public void addEngineBuildDir(String dirpath) {
        engineBuildDirs.add(dirpath);
    }

    // Returns the command line specified property files
    public List<String> getPropertyFiles() {
        return propertyFiles;
    }

    public List<IResource> getPropertyFilesAsResources() {
        List<IResource> resources = new ArrayList<>();
        for (String propertyFile : propertyFiles) {
            Path path = Paths.get(propertyFile);
            if (!path.isAbsolute()) {
                Path rootDir = Paths.get(getRootDirectory()).normalize().toAbsolutePath();
                Path settingsFile = path.normalize().toAbsolutePath();
                Path relativePath = rootDir.relativize(settingsFile);
                resources.add(fileSystem.get(relativePath.toString()));
            }
            else
            {
                resources.add(fileSystem.get(path.getFileName().toString()));
            }
        }
        return resources;
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
            TimeProfiler.start("loadProjectFile");
            loadProjectFile(true);
            TimeProfiler.stop();

            String title = projectProperties.getStringValue("project", "title");
            if (title != null && title.isEmpty()) {
                throw new Exception("`project.title` in `game.project` must be non-empty.");
            }
            return doBuild(monitor, commands);
        } catch (CompileExceptionError e) {

            String s = Bob.logExceptionToString(MultipleCompileException.Info.SEVERITY_ERROR, e.getResource(), e.getLineNumber(), e.toString());
            if (s.contains("NullPointerException")) {
                e.printStackTrace(System.err); // E.g. when we happen to do something bad when handling exceptions
            }

            System.err.println(s);
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

        // This code is a quick way to allow for settings files from outside of a project
        // Those settings files are required to be using absolute paths.
        // Caveat is that we're mounting the folder, which may contain other files
        // TODO: Add way to insert specific (or virtual) files into the file system
        Set<String> mounts = new HashSet<>();
        for (String propertyFile : propertyFiles) {
            Path path = Paths.get(propertyFile);
            if (path.isAbsolute()) {
                String normalizedRoot = path.getParent().normalize().toString();
                // do not add the same mount twice if multiple settings are passed on commandline in same directory
                if (!mounts.contains(normalizedRoot)) {
                    DefaultFileSystem fs = new DefaultFileSystem();
                    fs.setRootDirectory(normalizedRoot);
                    this.fileSystem.addMountPoint(new FileSystemMountPoint(this.fileSystem, fs));
                    mounts.add(normalizedRoot);
                }
            }
        }

        Map<String, File> libFiles = LibraryUtil.collectLibraryFiles(getLibPath(), this.libUrls);
        if (libFiles == null) {
            throw new CompileExceptionError("Missing libraries folder. You need to run the 'resolve' command first!");
        }
        boolean missingFiles = false;

        for (String url : libFiles.keySet() ) {
            File file = libFiles.get(url);

            if (file != null && file.exists()) {
                this.fileSystem.addMountPoint(new ZipMountPoint(this.fileSystem, file.getAbsolutePath()));
            } else {
                missingFiles = true;
            }
        }
        BuildInputDataCollector.setDependencies(libFiles);
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
        for (Task t : this.getTasks()) {
            List<IResource> inputs = t.getInputs();
            List<IResource> outputs = t.getOutputs();
            for (IResource output : outputs) {
                String outStr = output.toString();
                boolean isGenerated = outStr.contains("_generated_");
                if (build_map.containsKey(outStr) && !isGenerated) {
                    List<IResource> inputsStored = build_map.get(outStr);
                    String errMsg = "Conflicting output resource '" + outStr + "' generated by the following input files: " + inputs.toString() + " <-> " + inputsStored.toString();
                    IResource errRes = getConflictingResource(output, inputs, inputsStored);
                    throw new CompileExceptionError(errRes, 0, errMsg);
                }
                build_map.put(outStr, inputs);
            }
        }
    }

    private Class<? extends IBundler> getBundlerClass(Platform platform) {
        for (Class<? extends IBundler> klass : bundlerClasses) {
            BundlerParams bundlerParams = klass.getAnnotation(BundlerParams.class);
            if (bundlerParams == null) {
                logWarning("Bundler class '%s' has no BundlerParams", klass.getName());
                continue;
            }
            for (String supportedPlatform : bundlerParams.platforms()) {
                if (platform.matchesPair(supportedPlatform))
                    return klass;
            }
        }
        return null;
    }

    public IBundler createBundler(Platform platform) throws CompileExceptionError {
        Class<? extends IBundler> bundlerClass = getBundlerClass(platform);
        if (bundlerClass == null) {
            throw new CompileExceptionError(null, -1, String.format("No bundler registered for platform %s", platform.getPair()));
        }

        try {
            return bundlerClass.newInstance();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private void bundle(IProgress monitor) throws IOException, CompileExceptionError {
        IProgress m = monitor.subProgress(1);
        m.beginTask(IProgress.Task.BUNDLING, 1);

        Platform platform = getPlatform();
        IBundler bundler = createBundler(platform);

        File bundleDir = getBundleOutputDirectory();
        BundleHelper.throwIfCanceled(monitor);
        bundleDir.mkdirs();
        bundler.bundleApplication(this, platform, bundleDir, monitor);
        String defoldSdk = this.option("defoldsdk", EngineVersion.sha1);
        BuildInputDataCollector.saveDataAsJson(getRootDirectory(), bundleDir, defoldSdk);
        m.worked(1);
        m.done();
    }

    private File getBundleOutputDirectory() {
        String bundleOutput = option("bundle-output", null);
        File bundleDir = null;
        if (bundleOutput != null) {
            bundleDir = new File(bundleOutput);
        } else {
            bundleDir = new File(getRootDirectory(), getBuildDirectory());
        }
        return bundleDir;
    }

    public void registerTextureCompressors() {
        for (Class<? extends ITextureCompressor> klass : textureCompressorClasses) {
            try {
                TextureCompression.registerCompressor(klass.newInstance());
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
        textureCompressorClasses.clear();
    }

    private Class<? extends IShaderCompiler> getShaderCompilerClass(Platform platform) {
        for (Class<? extends IShaderCompiler> klass : shaderCompilerClasses) {
            BundlerParams bundlerParams = klass.getAnnotation(BundlerParams.class);
            if (bundlerParams == null) {
                continue;
            }
            for (String supportedPlatform : bundlerParams.platforms()) {
                if (platform.matchesPair(supportedPlatform))
                    return klass;
            }
        }
        return null;
    }

    public IShaderCompiler getShaderCompiler(Platform platform) throws CompileExceptionError {
        // Look for a shader compiler plugin for this platform
        Class<? extends IShaderCompiler> shaderCompilerClass = getShaderCompilerClass(platform);
        if (shaderCompilerClass != null) {
            try {
                return shaderCompilerClass.newInstance();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        // If not found, try to get a built-in shader compiler for this platform
        IShaderCompiler commonShaderCompiler = ShaderCompilers.GetCommonShaderCompiler(platform);
        if (commonShaderCompiler != null) {
            return commonShaderCompiler;
        }

        throw new CompileExceptionError(null, -1, String.format("No shader compiler registered for platform %s", platform.getPair()));
    }

    private boolean anyFailing(Collection<TaskResult> results) {
        for (TaskResult taskResult : results) {
            if (!taskResult.isOk()) {
                return true;
            }
        }

        return false;
    }

    public List<Platform> getArchitectures() throws CompileExceptionError {
        Platform p = getPlatform();
        return Platform.getArchitecturesFromString(option("architectures", ""), p);
    }

    public Platform getPlatform() throws CompileExceptionError {
        String pair = option("platform", null);
        Platform p = Platform.getHostPlatform();
        if (pair != null) {
            // backwards compatibility.
            // TODO: remove in some future update
            if (pair.equals("x86_64-darwin"))
            {
                String deprecatedPair = pair;
                pair = Platform.X86_64MacOS.getPair();
                System.out.printf("Platform name %s is deprecated. Please use '%s' instead\n", deprecatedPair, pair);
            }
            else if (pair.equals("arm64-darwin"))
            {
                String deprecatedPair = pair;
                pair = Platform.Arm64Ios.getPair();
                System.out.printf("Platform name %s is deprecated. Please use '%s' instead\n", deprecatedPair, pair);
            }
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
        if (p == Platform.Arm64Ios || p == Platform.JsWeb || p == Platform.WasmWeb || p == Platform.WasmPthreadWeb || p == Platform.Armv7Android || p == Platform.Arm64Android)
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

    public void buildEnginePlatform(IProgress monitor, File buildDir, File cacheDir, Map<String,String> appmanifestOptions, Platform platform) throws IOException, CompileExceptionError, MultipleCompileException {

        // Get SHA1 and create log file
        final String sdkVersion = this.option("defoldsdk", EngineVersion.sha1);

        final String variant = appmanifestOptions.get("baseVariant");

        BundleHelper helper = new BundleHelper(this, platform, buildDir, variant, null);

        List<ExtenderResource> allSource = ExtenderUtil.getExtensionSources(this, platform, appmanifestOptions);

        allSource.addAll(helper.writeExtensionResources(platform));

        // Replace the unresolved manifests with the resolved ones
        List<ExtenderResource> resolvedManifests = helper.writeManifestFiles(platform, helper.getTargetManifestDir(platform));
        for (ExtenderResource manifest : resolvedManifests) {
            ExtenderResource src = null;
            for (ExtenderResource s : allSource) {
                if (s.getPath().equals(manifest.getPath())) {
                    src = s;
                    break;
                }
            }
            if (src != null) {
                allSource.remove(src);
            }
            allSource.add(manifest);
        }

        boolean debugUploadZip = this.hasOption("debug-ne-upload");

        if (debugUploadZip) {
            File debugZip = new File(buildDir.getParent(), "upload.zip");
            try (ZipOutputStream zipOut = new ZipOutputStream(Files.newOutputStream(debugZip.toPath()))) {
                ExtenderUtil.writeResourcesToZip(allSource, zipOut);
                System.out.printf("Wrote debug upload zip file to: %s", debugZip);
            } catch (Exception e) {
                throw new CompileExceptionError(String.format("Failed to write debug zip file to %s", debugZip), e);
            }
        }

        // Located in the same place as the log file in the unpacked successful build
        File logFile = new File(buildDir, "log.txt");
        String serverURL = this.option("build-server", "https://build.defold.com");

        try {
            ExtenderClient extender = new ExtenderClient(serverURL, cacheDir);
            extender.setHeaders(buildServerHeaders);

            String buildPlatform = platform.getExtenderPair();
            File zip = BundleHelper.buildEngineRemote(this, extender, buildPlatform, sdkVersion, allSource, logFile);

            cleanEngine(platform, buildDir);

            BundleHelper.unzip(Files.newInputStream(zip.toPath()), buildDir.toPath());
        } catch (ConnectException e) {
            throw new CompileExceptionError(String.format("Failed to connect to %s: %s", serverURL, e.getMessage()), e);
        } catch (ExtenderClientException e) {
            throw new CompileExceptionError(String.format("Failed to build engine: %s", e.getMessage()), e);
        }
    }

    public void buildLibraryPlatform(IProgress monitor, File buildDir, File cacheDir, Map<String,String> appmanifestOptions, Platform platform) throws IOException, CompileExceptionError, MultipleCompileException {

        // Get SHA1 and create log file
        final String sdkVersion = this.option("defoldsdk", EngineVersion.sha1);

        final String libraryName = this.option("ne-output-name", "default");

        final String variant = appmanifestOptions.get("baseVariant");

        // Located in the same place as the log file in the unpacked successful build
        File logFile = new File(buildDir, "log.txt");
        String serverURL = this.option("build-server", "https://build.defold.com");

        //platforms /armv7-ios /context /flags
        Map<String, Object> compilerOptions = new HashMap<>();
        String DEFINES = getSystemEnv("DEFINES");
        if (DEFINES != null) {
            List<String> values = Arrays.asList(DEFINES.split(" "));
            compilerOptions.put("defines", values);
        }
        String CXXFLAGS = getSystemEnv("CXXFLAGS");
        if (CXXFLAGS != null) {
            List<String> values = Arrays.asList(CXXFLAGS.split(" "));
            compilerOptions.put("flags", values);
        }
        String INCLUDES = getSystemEnv("INCLUDES");
        if (INCLUDES != null) {
            List<String> values = Arrays.asList(INCLUDES.split(" "));
            compilerOptions.put("includes", values);
        }

        for (String path : engineBuildDirs) {
            File dir = new File(path);
            if (!dir.isDirectory()) {
                throw new IOException(String.format("'%s' is not a directory!", path));
            }
        }

        List<ExtenderResource> allSource = ExtenderUtil.getLibrarySources(this, platform, appmanifestOptions, compilerOptions, libraryName, engineBuildDirs);

        boolean debugUploadZip = this.hasOption("debug-ne-upload");
        if (debugUploadZip) {
            File debugZip = new File(buildDir.getParent(), "upload.zip");
            ZipOutputStream zipOut = null;
            try {
                zipOut = new ZipOutputStream(new FileOutputStream(debugZip));
                ExtenderUtil.writeResourcesToZip(allSource, zipOut);
                System.out.printf("Wrote debug upload zip file to: %s", debugZip);
            } catch (Exception e) {
                throw new CompileExceptionError(String.format("Failed to write debug zip file to %s", debugZip), e);
            } finally {
                zipOut.close();
            }
        }

        try {
            ExtenderClient extender = new ExtenderClient(serverURL, cacheDir);
            extender.setHeaders(buildServerHeaders);

            String buildPlatform = platform.getExtenderPair();
            File zip = BundleHelper.buildEngineRemote(this, extender, buildPlatform, sdkVersion, allSource, logFile);

            BundleHelper.unzip(new FileInputStream(zip), buildDir.toPath());
        } catch (ConnectException e) {
            throw new CompileExceptionError(String.format("Failed to connect to %s: %s", serverURL, e.getMessage()), e);
        } catch (ExtenderClientException e) {
            throw new CompileExceptionError(String.format("Failed to build engine: %s", e.getMessage()), e);
        }
    }

    public void buildEngine(IProgress monitor, String[] architectures, Map<String,String> appmanifestOptions) throws IOException, CompileExceptionError, MultipleCompileException {
        // Store the build one level above the content build since that folder gets removed during a distclean
        String internalDir = FilenameUtils.concat(rootDirectory, ".internal");
        File cacheDir = new File(FilenameUtils.concat(internalDir, "cache"));
        cacheDir.mkdirs();

        IProgress m = monitor.subProgress(architectures.length);
        m.beginTask(IProgress.Task.BUILDING_ENGINE, 0);

        // Build all skews of platform
        String outputDir = getBinaryOutputDirectory();

        ExecutorService buildEngineExecutor = Executors.newFixedThreadPool(architectures.length);
        List<Future<?>> buildEngineFutures = new ArrayList<>();

        for (int i = 0; i < architectures.length; ++i) {
            Platform platform = Platform.get(architectures[i]);

            String buildPlatform = platform.getExtenderPair();
            File buildDir = new File(FilenameUtils.concat(outputDir, buildPlatform));
            buildDir.mkdirs();

            buildEngineFutures.add(buildEngineExecutor.submit(() -> {
                TimeProfiler.start("Build Remote Engine %s", platform.toString());
                TimeProfiler.addData("withSymbols", appmanifestOptions.get("baseVariant"));
                TimeProfiler.addData("variant", appmanifestOptions.get("withSymbols"));
                boolean buildLibrary = shouldBuildArtifact("library");
                try {
                    if (buildLibrary) {
                        buildLibraryPlatform(monitor, buildDir, cacheDir, appmanifestOptions, platform);
                    } else {
                        buildEnginePlatform(monitor, buildDir, cacheDir, appmanifestOptions, platform);
                    }
                } catch (Throwable e) {
                    throw new RuntimeException(e);
                } finally {
                    TimeProfiler.stop();
                }
            }));
            m.worked(1);
        }

        for (Future<?> future : buildEngineFutures) {
            try {
                future.get();
            } catch (Exception e) {
                Throwable cause = e.getCause();
                if (cause.getCause() instanceof CompileExceptionError) {
                    throw (CompileExceptionError) cause.getCause();
                } else if (cause.getCause() instanceof MultipleCompileException) {
                    throw (MultipleCompileException) cause.getCause();
                } else {
                    throw new RuntimeException(e);
                }
            }
        }
        buildEngineExecutor.shutdown();
        m.done();
    }

    private static boolean deleteDirectory(File directoryToBeDeleted) {
        File[] allContents = directoryToBeDeleted.listFiles();
        if (allContents != null) {
            for (File file : allContents) {
                deleteDirectory(file);
            }
        }
        return directoryToBeDeleted.delete();
    }

    private void cleanEngine(Platform platform, File dir) throws IOException, CompileExceptionError {
        if (!dir.exists()) {
            return;
        }

        // Check for at least a previous built engine before triggering a recursive delete
        List<String> defaultNames = platform.formatBinaryName("dmengine");
        for (String defaultName : defaultNames) {
            File exe = new File(FilenameUtils.concat(dir.getAbsolutePath(), defaultName));
            if (exe.exists()) {
                Project.deleteDirectory(dir);
                break;
            }
        }
    }

    private void cleanEngines(IProgress monitor, String[] platformStrings) throws IOException, CompileExceptionError {
        IProgress m = monitor.subProgress(platformStrings.length);
        m.beginTask(IProgress.Task.CLEANING_ENGINE, 0);

        String outputDir = getBinaryOutputDirectory();
        for (int i = 0; i < platformStrings.length; ++i) {
            Platform platform = Platform.get(platformStrings[i]);
            cleanEngine(platform, new File(outputDir, platform.getExtenderPair()));
            m.worked(1);
        }

        m.done();
    }

    static void addToPath(String variable, String path) {
        String newPath = null;

        // Check if variable is set externally.
        if (System.getProperty(variable) != null) {
            newPath = System.getProperty(variable);
        }

        if (newPath == null) {
            // Set path where the shared library is found.
            newPath = path;
        } else {
            // Append path where the shared library is found.
            newPath += File.pathSeparator + path;
        }

        // Set the concatenated jna.library path
        System.setProperty(variable, newPath);
        logger.info("Set %s to '%s'", variable, newPath);
    }

    private void registerPipelinePlugins() throws CompileExceptionError {
        TimeProfiler.start("registerPipelinePlugins");
        // Find the plugins and register them now, before we're building the content
        BundleHelper.extractPipelinePlugins(this, getPluginsDirectory());
        List<File> plugins = BundleHelper.getPipelinePlugins(this, getPluginsDirectory());
        if (!plugins.isEmpty()) {
            logger.info("\nFound plugins:");
        }

        String hostPlatform = Platform.getHostPlatform().getExtenderPair();

        for (File plugin : plugins) {
            scanner.addUrl(plugin);

            File pluginsDir = plugin.getParentFile().getParentFile(); // The <extension>/plugins dir
            File libDir = new File(pluginsDir, "lib");
            File platformDir = new File(libDir, hostPlatform);

            if (platformDir.exists()) {
                addToPath("jna.library.path", platformDir.getAbsolutePath());
                addToPath("java.library.path", platformDir.getAbsolutePath());
            }

            String relativePath = new File(rootDirectory).toURI().relativize(plugin.toURI()).getPath();
            logger.info("  %s", relativePath);
        }
        logger.info("");
        TimeProfiler.stop();
    }

    private boolean shouldBuildArtifact(String artifact) {
        String str = this.option("build-artifacts", "");
        List<String> artifacts = Arrays.asList(str.split(","));
        return artifacts.contains(artifact);
    }

    private boolean shouldBuildEngine() {
        String str = this.option("build-artifacts", "");
        return str.equals("") || shouldBuildArtifact("engine");
    }

    public void scanJavaClasses() throws IOException, CompileExceptionError {
        createClassLoaderScanner();
        registerPipelinePlugins();
        scan(scanner, "com.dynamo.bob");
        scan(scanner, "com.defold.extension.pipeline");
    }

    private Future buildRemoteEngine(IProgress monitor, ExecutorService executor) {
        Callable<Void> callable = new Callable<Void>() {
            public Void call() throws Exception {
                logInfo("Build Remote Engine...");
                final String variant = option("variant", Bob.VARIANT_RELEASE);
                final Boolean withSymbols = hasOption("with-symbols");

                Map<String, String> appmanifestOptions = new HashMap<>();
                appmanifestOptions.put("baseVariant", variant);
                appmanifestOptions.put("withSymbols", withSymbols.toString());

                if (hasOption("build-artifacts")) {
                    String s = option("build-artifacts", "");
                    System.out.printf("build-artifacts: %s\n", s);
                    appmanifestOptions.put("buildArtifacts", s);
                }

                Platform platform = getPlatform();

                String[] architectures = platform.getArchitectures().getDefaultArchitectures();
                String customArchitectures = option("architectures", null);
                if (customArchitectures != null) {
                    architectures = customArchitectures.split(",");
                }

                long tstart = System.currentTimeMillis();

                try {
                    buildEngine(monitor, architectures, appmanifestOptions);
                }
                catch (Exception e) {
                    if ((e instanceof MultipleCompileException) ||
                        (e instanceof CompileExceptionError)) {
                        monitor.setCanceled(true);
                    }
                    throw  e;
                }

                long tend = System.currentTimeMillis();
                logger.info("Engine build took %f s", (tend-tstart)/1000.0);
                return (Void)null;
            }
        };
        return executor.submit(callable);
    }


    /**
     *  Options from the `game.project` file that may affect build outputs.
     */
    private static class GameProjectBuildOption {
        public String inputOption, outputOption, propertyCategory, propertyKey;
        public List<String> appManifestSymbols;
        /**
         * @param inputOption        Option that may be used with Bob.
         * @param outputOption       How the option will be saved in project options using project.setOption() for future use.
         * @param propertyCategory   Category in the `game.project` file.
         * @param propertyKey        Key in the `game.project` file.
         * @param appManifestSymbols Symbols from appManifest that makes this option true.
         */
        public GameProjectBuildOption(String inputOption, String outputOption, String propertyCategory, String propertyKey, List<String> appManifestSymbols) {
            this.inputOption = inputOption;
            this.outputOption = outputOption;
            this.propertyCategory = propertyCategory;
            this.propertyKey = propertyKey;
            this.appManifestSymbols = appManifestSymbols;
        }
    }

    public void configurePreBuildProjectOptions() throws IOException, CompileExceptionError {
        TimeProfiler.start("configurePreBuildProjectOptions");
        List<GameProjectBuildOption> options = new ArrayList<>();
        options.add(new GameProjectBuildOption("debug-output-spirv", "output-spirv", "shader","output_spirv",List.of("GraphicsAdapterVulkan")));
        options.add(new GameProjectBuildOption("debug-output-hlsl", "output-hlsl", "shader","output_hlsl",List.of("GraphicsAdapterDX12")));
        options.add(new GameProjectBuildOption("debug-output-wgsl", "output-wgsl", "shader","output_wgsl",List.of("GraphicsAdapterWebGPU")));
        options.add(new GameProjectBuildOption("debug-output-msl", "output-msl", "shader","output_msl",List.of("GraphicsAdapterMetal")));
        options.add(new GameProjectBuildOption("debug-output-glsl", "output-glsl", "shader","output_glsl",List.of("GraphicsAdapterOpenGL", "GraphicsAdapterOpenGLES")));
        options.add(new GameProjectBuildOption("output-glsles100", "output-glsles100", "shader","output_glsl_es100",null));
        options.add(new GameProjectBuildOption("output-glsles300", "output-glsles300", "shader","output_glsl_es300",null));
        options.add(new GameProjectBuildOption("output-glsl120", "output-glsl120", "shader","output_glsl120",null));
        options.add(new GameProjectBuildOption("output-glsl330", "output-glsl330", "shader","output_glsl330",null));
        options.add(new GameProjectBuildOption("output-glsl430", "output-glsl430", "shader","output_glsl430",null));
        options.add(new GameProjectBuildOption("exclude-gles-sm100", "exclude-gles-sm100", "shader", "exclude_gles_sm100", null));

        options.add(new GameProjectBuildOption("sound-stream-enabled", "sound-stream-enabled", "sound","stream_enabled",null));
        options.add(new GameProjectBuildOption("model-split-large-meshes", "model-split-large-meshes", "model","split_meshes",null));
        options.add(new GameProjectBuildOption("prometheus-disabled", "prometheus-disabled", "prometheus","disabled",null));
        options.add(new GameProjectBuildOption("font-runtime-generation", "font-runtime-generation", "font","runtime_generation",null));

        Platform currentPlatform = getPlatform();
        final List<Platform> architectures = Platform.getArchitecturesFromString(this.option("architectures", ""), currentPlatform);
        Set<String> architectureSet = new HashSet<>();
        for (Platform platform : architectures) {
            architectureSet.add(platform.toString());
        }
        architectureSet.addAll(Arrays.asList(currentPlatform.getExtenderPaths()));
        List<Map<String, Object>> platformsSettings = new ArrayList<>();
        for(String arch : architectureSet) {
            platformsSettings.add(ExtenderUtil.getPlatformSettings(this, arch));
        }

        for(GameProjectBuildOption option:options) {
            boolean fromProjectProperties = this.getProjectProperties().getBooleanValue(option.propertyCategory, option.propertyKey, false);
            if (this.hasOption(option.inputOption)) {
                boolean fromProjectOptions = this.option(option.inputOption, "false").equals("true");
                this.setOption(option.outputOption, Boolean.toString(fromProjectProperties || fromProjectOptions));
            } else if (option.appManifestSymbols != null) {
                boolean hasSymbol = false;
                for(Map<String, Object>platfromSetting : platformsSettings) {
                    for (String optionSymbol : option.appManifestSymbols) {
                        hasSymbol = hasSymbol || ExtenderUtil.hasSymbol(optionSymbol, platfromSetting);
                    }
                }
                this.setOption(option.outputOption, Boolean.toString(hasSymbol || fromProjectProperties));
            } else {
                this.setOption(option.outputOption, Boolean.toString(fromProjectProperties));
            }
        }

        boolean isPhysics2D = this.getProjectProperties().getStringValue("physics", "type", "2D").equals("2D");
        this.setOption("physics-type-2D", Boolean.toString(isPhysics2D));
        TimeProfiler.stop();
    }

    private ArrayList<TextureCompressorPreset> parseTextureCompressorPresetFromJSON(String fromPath, byte[] data) throws CompileExceptionError {
        ObjectMapper objectMapper = new ObjectMapper();
        ArrayList<TextureCompressorPreset> presets = new ArrayList<>();

        try {
            JsonNode rootNode = objectMapper.readTree(data);
            String compressorName = rootNode.get("name").asText();

            JsonNode presetsArray = rootNode.get("presets");
            if (presetsArray.isArray()) {
                for (JsonNode jsonPreset : presetsArray) {
                    // Access preset fields
                    String presetName = jsonPreset.get("name").asText();

                    TextureCompressorPreset preset = new TextureCompressorPreset(presetName, presetName, compressorName);

                    JsonNode argsNode = jsonPreset.get("args");
                    Iterator<Map.Entry<String, JsonNode>> fields = argsNode.getFields();
                    while (fields.hasNext()) {
                        Map.Entry<String, JsonNode> field = fields.next();
                        String key = field.getKey();
                        JsonNode value = field.getValue();

                        // Handle only int, float, and string types
                        if (value.isInt()) {
                            preset.setOptionInt(key, value.asInt());
                        } else if (value.isDouble() || value.isFloatingPointNumber()) {
                            preset.setOptionFloat(key, (float) value.asDouble());
                        } else if (value.isTextual()) {
                            preset.setOptionString(key, value.asText());
                        } else {
                            throw new CompileExceptionError("Error processing texture compressor preset from " + fromPath + ", unsupported type for key '" + key + "'");
                        }
                    }

                    presets.add(preset);
                }
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
        return presets;
    }

    // Look for all files in the project named "something.texc_json"
    // and parse them into TextureCompressorPreset files that gets installed into
    // the texture compressor handler.
    private void installTextureCompressorPresets() throws IOException, CompileExceptionError {
        ArrayList<String> paths = new ArrayList<>();
        findResourcePathsByExtension("", ".texc_json", paths);

        for (String p : paths) {
            IResource r = getResource(p);

            if (r.isFile()) {
                ArrayList<TextureCompressorPreset> presets = parseTextureCompressorPresetFromJSON(r.getPath(), r.getContent());
                for (TextureCompressorPreset preset : presets) {
                    logger.info("Installing texture compression preset from file: " + r.getPath());
                    TextureCompression.registerPreset(preset);
                }
            }
        }
    }

    private void transpileLua(IProgress monitor) throws CompileExceptionError, IOException {
        List<ILuaTranspiler> transpilers = PluginScanner.getOrCreatePlugins("com.defold.extension.pipeline", ILuaTranspiler.class);
        if (transpilers != null) {
            IProgress transpilerProgress = monitor.subProgress(1);
            transpilerProgress.beginTask(IProgress.Task.TRANSPILING_TO_LUA, 1);
            for (ILuaTranspiler transpiler : transpilers) {
                IResource buildFileResource = getResource(transpiler.getBuildFileResourcePath());
                if (buildFileResource.exists()) {
                    String ext = "." + transpiler.getSourceExt();
                    ArrayList<IResource> sources = new ArrayList<>();
                    fileSystem.walk("", new FileSystemWalker() {
                        @Override
                        public void handleFile(String path, Collection<String> results) {
                            if (path.endsWith(ext)) {
                                sources.add(fileSystem.get(path));
                            }
                        }
                    }, new ArrayList<>());
                    if (!sources.isEmpty()) {
                        // We transpile to lua from the project dir only if all the source code files exist on disc. Since
                        // some source file may come as dependencies in zip archives, the transpiler will not be able to
                        // transpile them. In this situation, we extract all source files to a temporary folder. Similar
                        // logic is implemented in editor in editor.code.transpilers/produce-build-output function
                        boolean useProjectDir = buildFileResource instanceof DefaultResource && sources.stream().allMatch(s -> s instanceof DefaultResource);
                        File sourceDir;
                        if (useProjectDir) {
                            sourceDir = new File(rootDirectory);
                        } else {
                            sourceDir = Files.createTempDirectory("tr-" + transpiler.getClass().getSimpleName()).toFile();
                            buildFileResource.getContent();
                            File buildFile = new File(sourceDir, buildFileResource.getPath());
                            Path buildFilePath = buildFile.toPath();
                            Files.write(buildFilePath, buildFileResource.getContent());
                            Files.setLastModifiedTime(buildFilePath, FileTime.fromMillis(buildFileResource.getLastModified()));
                            for (IResource source : sources) {
                                Path sourcePath = new File(sourceDir, source.getPath()).toPath();
                                Files.write(sourcePath, source.getContent());
                                Files.setLastModifiedTime(sourcePath, FileTime.fromMillis(source.getLastModified()));
                            }
                        }
                        File outputDir = new File(rootDirectory, "build/tr/" + transpiler.getClass().getSimpleName());
                        Files.createDirectories(outputDir.toPath());
                        try {
                            List<ILuaTranspiler.Issue> issues = transpiler.transpile(new File(getPluginsDirectory()), sourceDir, outputDir);
                            List<ILuaTranspiler.Issue> errors = issues.stream().filter(issue -> issue.severity == ILuaTranspiler.Severity.ERROR).collect(Collectors.toList());
                            if (!errors.isEmpty()) {
                                MultipleCompileException exception = new MultipleCompileException("Transpilation failed", null);
                                errors.forEach(issue -> exception.addIssue(issue.severity.ordinal(), getResource(issue.resourcePath), issue.message, issue.lineNumber));
                                throw exception;
                            } else {
                                issues.forEach(issue -> {
                                    Level level = switch (issue.severity) {
                                        case INFO -> Level.INFO;
                                        case WARNING -> Level.WARNING;
                                        default -> throw new IllegalStateException();
                                    };
                                    logger.log(level, issue.resourcePath + ":" + issue.lineNumber + ": " + issue.message);
                                });
                            }
                            DefaultFileSystem fs = new DefaultFileSystem();
                            fs.setRootDirectory(outputDir.toString());
                            ArrayList<String> results = new ArrayList<>();
                            fs.walk("", new FileSystemWalker() {
                                @Override
                                public void handleFile(String path, Collection<String> results) {
                                    if (path.endsWith(".lua")) {
                                        results.add(path);
                                    }
                                }
                            }, results);
                            fileSystem.addMountPoint(new FileSystemMountPoint(fileSystem, fs));
                        } catch (Exception e) {
                            throw new CompileExceptionError(buildFileResource, 1, "Transpilation failed", e);
                        } finally {
                            if (!useProjectDir) {
                                FileUtils.deleteDirectory(sourceDir);
                            }
                        }
                    }
                }
            }
            transpilerProgress.done();
        }
    }

    private List<TaskResult> createAndRunTasks(IProgress monitor) throws IOException, CompileExceptionError {
        // Do early test if report files are writable before we start building
        boolean generateReport = this.hasOption("build-report-json") || this.hasOption("build-report-html");
        FileWriter resourceReportJSONWriter = null;
        FileWriter resourceReportHTMLWriter = null;
        FileWriter excludedResourceReportJSONWriter = null;
        FileWriter excludedResourceReportHTMLWriter = null;

        if (this.hasOption("build-report-json")) {
            String resourceReportJSONPath = this.option("build-report-json", "report.json");

            File resourceReportJSONFile = new File(resourceReportJSONPath);
            File resourceReportJSONFolder = resourceReportJSONFile.getParentFile();
            if (resourceReportJSONFolder != null && !resourceReportJSONFolder.exists()) {
                boolean success = resourceReportJSONFolder.mkdirs();
                if (!success) {
                    throw new IOException("Failed to create directories for path: " + resourceReportJSONFolder.getAbsolutePath());
                }
            }
            resourceReportJSONWriter = new FileWriter(resourceReportJSONFile);

            String excludedResourceReportJSONName = "excluded_" + resourceReportJSONFile.getName();
            File excludedResourceReportJSONFile = new File(resourceReportJSONFolder, excludedResourceReportJSONName);
            excludedResourceReportJSONWriter = new FileWriter(excludedResourceReportJSONFile);
        }
        if (this.hasOption("build-report-html")) {
            String resourceReportHTMLPath = this.option("build-report-html", "report.html");
            File resourceReportHTMLFile = new File(resourceReportHTMLPath);
            File parentDir = resourceReportHTMLFile.getParentFile();
            if (parentDir != null && !parentDir.exists()) {
                boolean success = parentDir.mkdirs();
                if (!success) {
                    throw new IOException("Failed to create directories for path: " + parentDir.getAbsolutePath());
                }
            }
            File resourceReportHTMLFolder = resourceReportHTMLFile.getParentFile();
            resourceReportHTMLWriter = new FileWriter(resourceReportHTMLFile);

            String excludedResourceReportHTMLName = "excluded_" + resourceReportHTMLFile.getName();
            File excludedResourceReportHTMLFile = new File(resourceReportHTMLFolder, excludedResourceReportHTMLName);
            excludedResourceReportHTMLWriter = new FileWriter(excludedResourceReportHTMLFile);
        }

        IProgress m = monitor.subProgress(99);
        TimeProfiler.start("ensureBobInitialized");
        Bob.ensureBobInitialized();
        TimeProfiler.stop();
        IProgress mrep = m.subProgress(1);
        mrep.beginTask(IProgress.Task.READING_TASKS, 1);
        TimeProfiler.start("Create tasks");
        BundleHelper.throwIfCanceled(monitor);
        createTasks();
        validateBuildResourceMapping();
        TimeProfiler.addData("TasksCount", tasks.size());
        TimeProfiler.stop();
        mrep.done();

        BundleHelper.throwIfCanceled(monitor);
        m.beginTask(IProgress.Task.BUILDING, tasks.size());
        BundleHelper.throwIfCanceled(monitor);
        List<TaskResult> result = runTasks(m);
        BundleHelper.throwIfCanceled(monitor);
        m.done();

        // Generate and save build report
        TimeProfiler.start("Generating build size report");
        if (generateReport && !anyFailing(result)) {
            mrep = monitor.subProgress(1);
            mrep.beginTask(IProgress.Task.GENERATING_REPORT, 1);
            ReportGenerator rg = new ReportGenerator(this);
            String resourceReportJSON = rg.generateResourceReportJSON();
            String excludedResourceReportJSON = rg.generateExcludedResourceReportJSON();

            // Save JSON report
            if (this.hasOption("build-report-json")) {
                resourceReportJSONWriter.write(resourceReportJSON);
                resourceReportJSONWriter.close();
                excludedResourceReportJSONWriter.write(excludedResourceReportJSON);
                excludedResourceReportJSONWriter.close();
            }

            // Save HTML report
            if (this.hasOption("build-report-html")) {
                String resourceReportHTML = rg.generateHTML(resourceReportJSON);
                String excludedResourceReportHTML = rg.generateHTML(excludedResourceReportJSON);
                resourceReportHTMLWriter.write(resourceReportHTML);
                resourceReportHTMLWriter.close();
                excludedResourceReportHTMLWriter.write(excludedResourceReportHTML);
                excludedResourceReportHTMLWriter.close();
            }
            mrep.done();
        }
        TimeProfiler.stop();

        return result;
    }

    private void clean(IProgress monitor, State state) {
        IProgress m = monitor.subProgress(1);
        List<String> paths = state.getPaths();
        m.beginTask(IProgress.Task.CLEANING, paths.size());
        for (String path : paths) {
            File f = new File(path);
            if (f.exists()) {
                state.removeSignature(path);
                f.delete();
                m.worked(1);
                BundleHelper.throwIfCanceled(monitor);
            }
        }
        m.done();
    }

    private void distClean(IProgress monitor) throws IOException {
        IProgress m = monitor.subProgress(1);
        m.beginTask(IProgress.Task.CLEANING, 1);
        BundleHelper.throwIfCanceled(monitor);
        FileUtils.deleteDirectory(new File(FilenameUtils.concat(rootDirectory, buildDirectory)));
        m.worked(1);
        m.done();
    }

    private List<TaskResult> doBuild(IProgress monitor, String... commands) throws Throwable, IOException, CompileExceptionError, MultipleCompileException {
        TimeProfiler.start("Prepare cache");
        resourceCache.init(getLocalResourceCacheDirectory(), getRemoteResourceCacheDirectory());
        resourceCache.setRemoteAuthentication(getRemoteResourceCacheUser(), getRemoteResourceCachePass());
        fileSystem.loadCache();
        IResource stateResource = fileSystem.get(FilenameUtils.concat(buildDirectory, "_BobBuildState_"));
        state = State.load(stateResource);
        TimeProfiler.stop();
        List<TaskResult> result = new ArrayList<TaskResult>();

        BundleHelper.throwIfCanceled(monitor);

        monitor.beginTask(IProgress.Task.WORKING, 100);
        // it should be done before scanJavaClasses to have updated options
        configurePreBuildProjectOptions();
        {
            TimeProfiler.start("scanJavaClasses");
            IProgress mrep = monitor.subProgress(1);
            mrep.beginTask(IProgress.Task.READING_CLASSES, 1);
            scanJavaClasses();
            mrep.done();
            TimeProfiler.stop();
        }

        List<IPlugin> plugins = new ArrayList<>();
        for (Class<? extends IPlugin> klass : pluginClasses) {
            IPlugin plugin = klass.getConstructor().newInstance();
            plugin.init(this);
            plugins.add(plugin);
        }

        boolean texture_compress = this.option("texture-compression", "false").equals("true");
        if (texture_compress)
        {
            registerTextureCompressors();
        }

        loop:
        for (String command : commands) {
            BundleHelper.throwIfCanceled(monitor);
            TimeProfiler.start(command);
            switch (command) {
                case "build": {
                    TimeProfiler.start("PrepExtensions");
                    ExtenderUtil.checkProjectForDuplicates(this); // Throws if there are duplicate files in the project (i.e. library and local files conflict)
                    final String[] platforms = getPlatformStrings();
                    Future<Void> remoteBuildFuture = null;
                    // Get or build engine binary
                    boolean shouldBuildRemoteEngine = ExtenderUtil.hasNativeExtensions(this);
                    boolean shouldBuildProject = shouldBuildEngine() && BundleHelper.isArchiveIncluded(this);
                    TimeProfiler.stop();

                    if (shouldBuildProject) {
                        // do this before buildRemoteEngine to prevent concurrent modification exception, since
                        // lua transpilation adds new mounts with compiled Lua that buildRemoteEngine iterates over
                        // when sending to extender
                        TimeProfiler.start("transpileLua");
                        transpileLua(monitor);
                        TimeProfiler.stop();

                        TimeProfiler.start("installTextureCompressorPresets");
                        installTextureCompressorPresets();
                        TimeProfiler.stop();
                    }

                    TimeProfiler.start("PrepEngine");
                    TimeProfiler.addData("shouldBuildRemoteEngine", shouldBuildRemoteEngine);
                    if (shouldBuildRemoteEngine) {
                        remoteBuildFuture = buildRemoteEngine(monitor, executor);
                    }
                    else {
                        // Remove the remote built executables in the build folder, they're still in the cache
                        cleanEngines(monitor, platforms);
                        if (hasOption("with-symbols")) {
                            IProgress progress = monitor.subProgress(1);
                            EngineArtifactsProvider.downloadSymbols(this, progress);
                            progress.done();
                        }
                    }
                    TimeProfiler.stop();
                    boolean resourceBuildingFailed = false;
                    try {
                        if (shouldBuildProject) {
                            result = createAndRunTasks(monitor);
                        }
                    }
                    catch(Exception e) {
                        if (!monitor.isCanceled()) {
                            resourceBuildingFailed = true;
                            throw e;
                        }
                    }
                    finally {
                        if (remoteBuildFuture != null && !resourceBuildingFailed) {
                            // get the result from the remote build and catch
                            // if an exception was thrown in buildRemoteEngine() the
                            // original exception is included in the ExecutionException
                            try {
                                remoteBuildFuture.get();
                            }
                            catch (ExecutionException|InterruptedException e) {
                                Throwable cause = e.getCause();
                                if ((cause instanceof MultipleCompileException) ||
                                    (cause instanceof CompileExceptionError)) {
                                    throw cause;
                                }
                                else {
                                    throw new CompileExceptionError(cause);
                                }
                            }
                        }
                        if (anyFailing(result)) {
                            break loop;
                        }
                    }
                    break;
                }
                case "clean": {
                    clean(monitor, state);
                    break;
                }
                case "distclean": {
                    distClean(monitor);
                    break;
                }
                case "bundle": {
                    bundle(monitor);
                    break;
                }
                default: break;
            }
            TimeProfiler.stop();
        }

        for (IPlugin plugin : plugins) {
            plugin.exit(this);
        }
        plugins.clear();

        monitor.done();
        TimeProfiler.start("Save cache");
        state.save(stateResource);
        fileSystem.saveCache();
        TimeProfiler.stop();
        return result;
    }




    @SuppressWarnings({ "rawtypes", "unchecked" })
    private List<TaskResult> runTasks(IProgress monitor) throws IOException, CompileExceptionError {

        // tasks are now built in parallel which means that we no longer want
        // to use all the cores just for texture generation
        TextureGenerator.maxThreads = getHalfThreads();

        TaskBuilder taskBuilder = new TaskBuilder(getTasks(), this);

        // create mapping between output and flags
        outputs.clear();
        for (IResource res : taskBuilder.getAllOutputs()) {
            outputs.put(res.getAbsPath(), EnumSet.noneOf(OutputFlags.class));
        }

        // build all tasks and make sure no new tasks were created while building
        tasks.clear();
        List<TaskResult> result = taskBuilder.build(monitor);
        if (!tasks.isEmpty()) {
            throw new CompileExceptionError("New tasks were created while tasks were building");
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

    public EnumSet<OutputFlags> getOutputFlags(String resourcePath) {
        return outputs.get(resourcePath);
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
     *
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
            Map<String, File> libFiles = LibraryUtil.collectLibraryFiles(libPath, libUrls);
            int count = this.libUrls.size();
            IProgress subProgress = progress.subProgress(count);
            subProgress.beginTask(IProgress.Task.DOWNLOADING_ARCHIVES, count);
            logInfo("Downloading %d archive(s)", count);

            // Use a fixed thread pool with 2 threads for parallel downloads
            ExecutorService executor = Executors.newFixedThreadPool(2);
            List<Future<Void>> futures = new ArrayList<>();

            for (int i = 0; i < count; ++i) {
                final int index = i;
                final URL url = libUrls.get(i);
                final File f = libFiles.get(url.toString());

                Future<Void> future = executor.submit(() -> {
                    try {
                        TimeProfiler.start("Lib %2d", index);
                        BundleHelper.throwIfCanceled(progress);

                        logInfo("%2d: Downloading %s", index, url);
                        TimeProfiler.addData("url", url.toString());
                        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
                        // GitLab will respond with a 406 Not Acceptable if the request
                        // is made without an Accept header
                        connection.setRequestProperty("Accept", "application/zip");

                        String etag = null;
                        if (f != null) {
                            String etagB64 = LibraryUtil.getETagFromName(LibraryUtil.getHashedUrl(url), f.getName());
                            if (etagB64 != null) {
                                etag = new String(new Base64().decode(etagB64.getBytes())).replace("\"", ""); // actually includes the quotation marks
                                etag = String.format("\"%s\"", etag); // fixing broken etag
                                connection.addRequestProperty("If-None-Match", etag);
                            }
                        }

                        // Check if URL contains basic auth credentials
                        String basicAuthData = null;
                        try {
                            URI uri = new URI(url.toString());
                            basicAuthData = uri.getUserInfo();
                        } catch (URISyntaxException e1) {
                            // Ignored, could not get URI and basic auth data from URL.
                        }

                        // Check if basic auth password is a token that should be replaced with
                        // an environment variable.
                        // The token should start and end with __ and exist as an environment
                        // variable.
                        if (basicAuthData != null) {
                            String[] parts = basicAuthData.split(":");
                            String username = parts[0];
                            String password = parts.length > 1 ? parts[1] : "";
                            if (password.startsWith("__") && password.endsWith("__")) {
                                String envKey = password.substring(2, password.length() - 2);
                                String envValue = getSystemEnv(envKey);
                                if (envValue != null) {
                                    basicAuthData = username + ":" + envValue;
                                }
                            }
                        }
                        // Pass correct headers along to server depending on auth alternative.
                        final String email = this.options.get("email");
                        final String auth = this.options.get("auth");
                        if (basicAuthData != null) {
                            String basicAuth = "Basic " + new String(new Base64().encode(basicAuthData.getBytes()));
                            connection.setRequestProperty("Authorization", basicAuth);
                        } else if (email != null && auth != null) {
                            connection.addRequestProperty("X-Email", email);
                            connection.addRequestProperty("X-Auth", auth);
                        }
                        InputStream input = null;
                        try {
                            connection.connect();
                            int code = connection.getResponseCode();

                            TimeProfiler.addData("status code", code);
                            if (code == 304) {
                                logInfo("%2d: Status %d: Already cached", index, code);
                            } else if (code >= 400) {
                                logWarning("%2d: Status %d: Failed to download %s", index, code, url);
                                throw new LibraryException(String.format("Status %d: Failed to download %s", code, url), new Exception());
                            } else {

                                String serverETag = connection.getHeaderField("ETag");
                                if (serverETag == null) {
                                    serverETag = connection.getHeaderField("Etag");
                                }

                                if (serverETag == null) {
                                    logWarning(String.format("The URL %s didn't provide an ETag", url));
                                    serverETag = "";
                                }

                                if (etag != null && !etag.equals(serverETag)) {
                                    logInfo("%2d: Status %d: ETag mismatch %s != %s. Deleting old file %s", index, code, etag != null ? etag : "", serverETag != null ? serverETag : "", f);
                                    f.delete();
                                    // Note: f becomes null in the original scope, but we need to handle this differently in parallel context
                                }

                                input = new BufferedInputStream(connection.getInputStream());

                                File targetFile = f;
                                if (targetFile == null) {
                                    targetFile = new File(libPath, LibraryUtil.getFileName(url, serverETag));
                                }
                                FileUtils.copyInputStreamToFile(input, targetFile);

                                try {
                                    ZipFile zip = new ZipFile(targetFile);
                                    zip.close();
                                } catch (ZipException e) {
                                    targetFile.delete();
                                    throw new LibraryException(String.format("The file obtained from %s is not a valid zip file", url.toString()), e);
                                }
                                logInfo("%2d: Status %d: Stored %s", index, code, targetFile);
                            }
                            connection.disconnect();
                        } catch (ConnectException e) {
                            throw new LibraryException(String.format("Connection refused by the server at %s", url.toString()), e);
                        } catch (FileNotFoundException e) {
                            throw new LibraryException(String.format("The URL %s points to a resource which doesn't exist", url.toString()), e);
                        } finally {
                            if (input != null) {
                                IOUtils.closeQuietly(input);
                            }
                            subProgress.worked(1);
                        }

                        BundleHelper.throwIfCanceled(subProgress);
                        TimeProfiler.stop();
                        return null;
                    } catch (Exception e) {
                        throw new RuntimeException(e);
                    }
                });
                futures.add(future);
            }
            // Wait for all downloads to complete
            for (Future<Void> future : futures) {
                try {
                    future.get();
                } catch (ExecutionException e) {
                    Throwable cause = e.getCause();
                    if (cause instanceof LibraryException) {
                        throw (LibraryException) cause;
                    } else if (cause instanceof IOException) {
                        throw (IOException) cause;
                    } else {
                        throw new LibraryException(cause.getMessage(), cause);
                    }
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    throw new LibraryException("Download interrupted", e);
                }
            }
            executor.shutdown();
        } catch (IOException ioe) {
            throw ioe;
        } catch (LibraryException le) {
            throw le;
        } catch (Exception e) {
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

    /**
     * Get the project build state
     * @return The project build state
     */
    public State getState() {
        return state;
    }

    /**
     * Get the resource cache for this project
     * @return The project resource cache
     */
    public ResourceCache getResourceCache() {
        return resourceCache;
    }

    public IResource getResource(String path) {
        return fileSystem.get(FilenameUtils.normalize(path, true));
    }

    public IResource getResource(String category, String key, boolean mustExist) throws IOException {
        IResource resource = null;
        String val = this.projectProperties.getStringValue(category, key);
        if (val != null && val.trim().length() > 0) {
            resource = this.getResource(val);
        }
        if (mustExist) {
            if (resource == null) {
                throw new IOException(String.format("Resource is null: %s.%s = '%s'", category, key, val==null?"null":val));
            }
            if (!resource.exists()) {
                throw new IOException(String.format("Resource does not exist: %s.%s = '%s'", category, key, resource.getPath()));
            }
        }
        return resource;
    }

    public IResource getResource(String category, String key) throws IOException {
        return getResource(category, key, true);
    }

    public IResource getGameProjectResource() {
        return getResource("/game.project");
    }

    public IResource getGeneratedResource(long hash, String suffix) {
        Map<Long, IResource> submap = hashToResource.get(suffix);
        if (submap == null)
            return null;
        return submap.get(hash);
    }

    public IResource createGeneratedResource(long hash, String suffix) {
        return createGeneratedResource(null, hash, suffix);
    }

    public IResource createGeneratedResource(String prefix, long hash, String suffix) {
        Map<Long, IResource> submap = hashToResource.get(suffix);
        if (submap == null) {
            submap = new HashMap<>();
            hashToResource.put(suffix, submap);
        }

        if (prefix == null) {
            prefix = "";
        }

        if (!prefix.isEmpty()) {
            prefix = "_" + prefix;
        }

        IResource genResource = fileSystem.get(String.format("_generated%s_%x.%s", prefix, hash, suffix)).output();
        submap.put(hash, genResource);
        return genResource;
    }

    public static String stripLeadingSlash(String path) {
        while (path.length() > 0 && path.charAt(0) == '/') {
            path = path.substring(1);
        }
        return path;
    }

    public static int getDefaultMaxCpuThreads() {
        int maxThreads = 1;
        int availableProcessors = Runtime.getRuntime().availableProcessors();
        if (availableProcessors > 4) {
            maxThreads = availableProcessors - 2;
        }
        else if (availableProcessors > 1) {
            maxThreads = availableProcessors - 1;
        }
        return maxThreads;
    }

    private void findResourcePathsByExtension(String _path, String ext, Collection<String> result) {
        TimeProfiler.start("findResourcePathsByExtension");
        TimeProfiler.addData("path", _path);
        TimeProfiler.addData("ext", ext);
        // Initialize and cache all paths only once
        if (allResourcePathsCache == null) {
            List<String> allPaths = new ArrayList<>();
            fileSystem.walk("", new FileSystemWalker() {
                public void handleFile(String filePath, Collection<String> results) {
                    results.add(FilenameUtils.normalize(filePath, true));
                }
            }, allPaths);
            allResourcePathsCache = allPaths;
        }
        _path = FilenameUtils.normalize(_path, true);
        _path = stripLeadingSlash(_path);
        // Fast path: if no filter, return everything
        if ((ext == null || ext.isEmpty()) && (_path == null || _path.isEmpty())) {
            result.addAll(allResourcePathsCache);
            TimeProfiler.stop();
            return;
        }
        IResource res = fileSystem.get(_path);
        try {
            if ( (_path != null && !_path.isEmpty()) && (!res.isFile() || res.getContent() == null)) {
                if (!_path.endsWith("/")) {
                    _path += "/";
                }
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
        final String path =_path;

        // Filter the cached list in parallel, collect safely, then add to result
        List<String> filteredPaths = allResourcePathsCache.parallelStream()
            .filter(p -> (ext == null || p.endsWith(ext)) &&
                         (path == null || path.isEmpty() || p.startsWith(path)))
            .collect(Collectors.toList());
        result.addAll(filteredPaths);
        TimeProfiler.stop();
    }

    public void findResourcePaths(String _path, Collection<String> result) {
        findResourcePathsByExtension(_path, null, result);
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

    public List<Task> getTasks() {
        return Collections.unmodifiableList(new ArrayList(this.tasks.values()));
    }

}
