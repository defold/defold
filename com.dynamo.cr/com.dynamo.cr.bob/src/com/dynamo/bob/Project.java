package com.dynamo.bob;

import static org.apache.commons.io.FilenameUtils.normalizeNoEndSeparator;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.net.URL;
import java.net.URLConnection;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.apache.commons.io.DirectoryWalker;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.fs.ClassLoaderMountPoint;
import com.dynamo.bob.fs.IFileSystem;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ZipMountPoint;

/**
 * Project abstraction. Contains input files, builder, tasks, etc
 * @author Christian Murray
 *
 */
public class Project {

    public final static String LIB_DIR = ".internal/lib";

    private IFileSystem fileSystem;
    private Map<String, Class<? extends Builder<?>>> extToBuilder = new HashMap<String, Class<? extends Builder<?>>>();
    private List<String> inputs = new ArrayList<String>();
    private ArrayList<Task<?>> newTasks;
    private State state;
    private String rootDirectory = ".";
    private String buildDirectory = "build";
    private Map<String, String> options = new HashMap<String, String>();
    private List<URL> libUrls = new ArrayList<URL>();

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

    /**
     * Build the project
     * @param monitor
     * @return list of {@link TaskResult}. Only executed nodes are part of the list.
     * @throws IOException
     * @throws CompileExceptionError
     */
    public List<TaskResult> build(IProgress monitor, String... commands) throws IOException, CompileExceptionError {
        try {
            return doBuild(monitor, commands);
        } catch (CompileExceptionError e) {
            // Pass on unmodified
            throw e;
        } catch (Throwable e) {
            throw new CompileExceptionError(null, 0, e.getMessage(), e);
        }
    }

    private void mount() throws IOException, CompileExceptionError {
        this.fileSystem.clearMountPoints();
        this.fileSystem.addMountPoint(new ClassLoaderMountPoint(this.fileSystem, "builtins/**"));
        List<File> libFiles = convertLibraryUrlsToFiles(this.libUrls);
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

    private List<TaskResult> doBuild(IProgress monitor, String... commands) throws IOException, CompileExceptionError {
        fileSystem.loadCache();
        IResource stateResource = fileSystem.get(FilenameUtils.concat(buildDirectory, "state"));
        state = State.load(stateResource);
        // resolve before mounting mount points
        for (String command : commands) {
            if (command.equals("resolve")) {
                resolve();
                break;
            }
        }
        // mount mount points before creating tasks
        mount();
        createTasks();
        List<TaskResult> result = new ArrayList<TaskResult>();

        monitor.beginTask("", 100);

        for (String command : commands) {
            if (command.equals("build")) {
                IProgress m = monitor.subProgress(99);
                m.beginTask("Building...", newTasks.size());
                result = runTasks(m);
                m.done();
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
            }
        }

        monitor.done();
        state.save(stateResource);
        fileSystem.saveCache();
        return result;
    }

    // Convert lib url into path, e.g. http://localhost:8080/test_lib.zip into LIB_PATH/test_lib.zip
    private String libUrlToPath(String libPath, URL url) {
        return FilenameUtils.concat(libPath, FilenameUtils.getName(url.getPath()));
    }

    private List<File> convertLibraryUrlsToFiles(List<URL> libUrls) {
        String libPath = getLibPath();
        List<File> files = new ArrayList<File>();
        for (URL url : libUrls) {
            files.add(new File(libUrlToPath(libPath, url)));
        }
        return files;
    }

    private void resolve() throws IOException, CompileExceptionError {
        String libPath = getLibPath();
        File libDir = new File(libPath);
        // Clean lib dir first
        FileUtils.deleteQuietly(libDir);
        FileUtils.forceMkdir(libDir);
        // Download libs
        List<File> libFiles = convertLibraryUrlsToFiles(libUrls);
        int count = this.libUrls.size();
        for (int i = 0; i < count; ++i) {
            URL url = libUrls.get(i);
            URLConnection connection = url.openConnection();
            connection.addRequestProperty("X-Email", this.options.get("email"));
            connection.addRequestProperty("X-Auth", this.options.get("auth"));
            FileUtils.copyInputStreamToFile(connection.getInputStream(), libFiles.get(i));
        }
    }

    @SuppressWarnings({ "rawtypes", "unchecked" })
    private List<TaskResult> runTasks(IProgress monitor) throws IOException {

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

    public void setLibUrls(List<URL> libUrls) {
        this.libUrls = libUrls;
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

    class Walker extends DirectoryWalker<String> {

        private Set<String> skipDirs;
        private ArrayList<String> result;

        public Walker(Set<String> skipDirs) {
            this.skipDirs = skipDirs;
        }

        public List<String> walk(String path) throws IOException {
            path = normalizeNoEndSeparator(path, true);

            result = new ArrayList<String>(1024);
            walk(new File(path), result);
            for (int i = 0; i < result.size(); ++i) {
                String relPath = result.get(i).substring(rootDirectory.length()+1);
                result.set(i, relPath);
            }
            return result;
        }

        @Override
        protected void handleFile(File file, int depth,
                Collection<String> results) throws IOException {
            String p = FilenameUtils.normalize(file.getPath(), true);

            String ext = "." + FilenameUtils.getExtension(p);
            Class<? extends Builder<?>> builderClass = extToBuilder.get(ext);
            if (builderClass != null)
                results.add(p);
        }

        @Override
        protected boolean handleDirectory(File directory, int depth,
                Collection<String> results) throws IOException {
            String path = FilenameUtils.normalize(directory.getPath(), true);
            for (String sd : skipDirs) {
                if (path.endsWith(sd)) {
                    return false;
                }
            }
            return super.handleDirectory(directory, depth, results);
        }

        public List<String> getResult() {
            return result;
        }
    }

    /**
     * Find source files
     * @param path path to begin in. Absolute or relative to root-directory
     * @param skipDirs
     * @throws IOException
     */
    public void findSources(String path, Set<String> skipDirs) throws IOException {
        if (!new File(path).isAbsolute()) {
            path = normalizeNoEndSeparator(FilenameUtils.concat(rootDirectory, path));
        }
        if (!new File(path).exists()) {
            throw new FileNotFoundException(String.format("the path '%s' can not be found", path));
        }
        Walker walker = new Walker(skipDirs);
        walker.walk(path);
        List<String> result = walker.getResult();
        inputs = result;
    }

    public IResource getResource(String path) {
        return fileSystem.get(path);
    }

    public List<Task<?>> getTasks() {
        return Collections.unmodifiableList(this.newTasks);
    }

}
