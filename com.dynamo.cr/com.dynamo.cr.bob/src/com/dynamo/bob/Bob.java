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

package com.dynamo.bob;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.URISyntaxException;
import java.net.URL;
import java.nio.file.AccessDeniedException;
import java.nio.file.FileAlreadyExistsException;
import java.nio.file.FileSystemException;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.StandardCopyOption;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.ArrayList;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.Option;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.archive.EngineVersion;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.LibraryUtil;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.TimeProfiler;
import com.dynamo.bob.cache.ResourceCacheKey;

public class Bob {

    public static final String VARIANT_DEBUG = "debug";
    public static final String VARIANT_RELEASE = "release";
    public static final String VARIANT_HEADLESS = "headless";

    private static boolean verbose = false;
    private static File rootFolder = null;
    private static boolean luaInitialized = false;

    public Bob() {
    }

    // Registers a shutdown hook to delete the temp folder
    public static void registerShutdownHook() {
        final File tmpDirFile = rootFolder;

        // Add shutdown hook; creates a runnable that will recursively delete files in the temp directory.
        Runtime.getRuntime().addShutdownHook(new Thread(
          new Runnable() {
            @Override
            public void run() {
                try {
                    FileUtils.deleteDirectory(tmpDirFile);
                } catch (IOException e) {
                    // DE 20181012
                    // DEF-3533 Building with Bob causes exception when cleaning temp directory
                    // Failing to delete the files is not fatal, but not 100% clean.
                    // On Win32 we fail to delete dlls that are loaded since the OS locks them and this code runs before
                    // the dlls are unloaded.
                    // There is no explicit API to unload DLLs in Java/JNI, to accomplish this we need to do the
                    // class loading for the native functions differently and use a more indirect calling convention for
                    // com.defold.libs.TexcLibrary.
                    // See https://web.archive.org/web/20140704120535/http://www.codethesis.com/blog/unload-java-jni-dll
                    //
                    // For now we just issue a warning that we don't fully clean up.
                    System.out.println("Warning: Failed to clean up temp directory '" + tmpDirFile.getAbsolutePath() + "'");
                }
                finally {
                    luaInitialized = false;
                }
            }
        }));
      }

    private static void init() {
        if (rootFolder != null) {
            return;
        }
        TimeProfiler.start("Create root folder");
        try {
            String envRootFolder = System.getenv("DM_BOB_ROOTFOLDER");
            if (envRootFolder != null) {
                rootFolder = new File(envRootFolder);
                if (!rootFolder.exists()) {
                    rootFolder.mkdirs();
                }
                if (!rootFolder.isDirectory()) {
                    throw new IOException(String.format("Error when specifying DM_BOB_ROOTFOLDER: %s is not a directory!", rootFolder.getAbsolutePath()));
                }
                System.out.println("env DM_BOB_ROOTFOLDER=" + rootFolder);
            } else {
                rootFolder = Files.createTempDirectory(null).toFile();
                // Make sure we remove the temp folder on exit
                registerShutdownHook();
            }
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        TimeProfiler.stop();
    }

    public static void initLua() {
        if (luaInitialized) {
            return;
        }
        init();
        try {
            extract(Bob.class.getResource("/lib/luajit-share.zip"), new File(rootFolder, "share"));
            luaInitialized = true;
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    public static void initAndroid() {
        init();
        try {
            // Android SDK aapt is dynamically linked against libc++.so, we need to extract it so that
            // aapt will find it later when AndroidBundler is run.
            String libc_filename = Platform.getHostPlatform().getLibPrefix() + "c++" + Platform.getHostPlatform().getLibSuffix();
            URL libc_url = Bob.class.getResource("/lib/" + Platform.getHostPlatform().getPair() + "/" + libc_filename);
            if (libc_url != null) {
                File f = new File(rootFolder, Platform.getHostPlatform().getPair() + "/lib/" + libc_filename);
                atomicCopy(libc_url, f, false);
            }

            extract(Bob.class.getResource("/lib/android-res.zip"), rootFolder);

            // NOTE: android.jar and classes.dex aren't are only available in "full bob", i.e. from CI
            URL android_jar = Bob.class.getResource("/lib/android.jar");
            if (android_jar != null) {
                File f = new File(rootFolder, "lib/android.jar");
                atomicCopy(android_jar, f, false);
            }
            URL classes_dex = Bob.class.getResource("/lib/classes.dex");
            if (classes_dex != null) {
                File f = new File(rootFolder, "lib/classes.dex");
                atomicCopy(classes_dex, f, false);
            }

        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private static void extract(final URL url, File toFolder) throws IOException {

        ZipInputStream zipStream = new ZipInputStream(new BufferedInputStream(url.openStream()));

        try{
            ZipEntry entry = zipStream.getNextEntry();
            while (entry != null)
            {
                if (!entry.isDirectory()) {

                    File dstFile = new File(toFolder, entry.getName());
                    dstFile.deleteOnExit();
                    dstFile.getParentFile().mkdirs();

                    OutputStream fileStream = null;

                    try {
                        final byte[] buf;
                        int i;

                        fileStream = new FileOutputStream(dstFile);
                        buf = new byte[1024];
                        i = 0;

                        while((i = zipStream.read(buf)) != -1) {
                            fileStream.write(buf, 0, i);
                        }
                    } finally {
                        IOUtils.closeQuietly(fileStream);
                    }
                    verbose("Extracted '%s' from '%s' to '%s'", entry.getName(), url, dstFile.getAbsolutePath());
                }

                entry = zipStream.getNextEntry();
            }
        } finally {
            IOUtils.closeQuietly(zipStream);
        }
    }

    public static String getPath(String path) {
        init();
        File f = new File(rootFolder, path);
        if (!f.exists()) {
            throw new RuntimeException(String.format("location %s not found", f.toString()));
        }
        return f.getAbsolutePath();
    }

    public static List<String> getExes(Platform platform, String name) throws IOException {
        String[] exeSuffixes = platform.getExeSuffixes();
        List<String> exes = new ArrayList<String>();
        for (String exeSuffix : exeSuffixes) {
            exes.add(getExeWithExtension(platform, name, exeSuffix));
        }
        return exes;
    }

    public static String getExe(Platform platform, String name) throws IOException {
        List<String> exes = getExes(platform, name);
        if (exes.size() > 1) {
            throw new IOException("More than one alternative when getting binary executable for platform: " + platform.toString());
        }
        return exes.get(0);
    }

    public static void unpackSharedLibraries(Platform platform, List<String> names) throws IOException {
        init();

        TimeProfiler.start("unpackSharedLibraries");
        String libSuffix = platform.getLibSuffix();
        for (String name : names) {
            TimeProfiler.start(name);
            String depName = platform.getPair() + "/" + name + libSuffix;
            File f = new File(rootFolder, depName);
            if (!f.exists()) {
                URL url = Bob.class.getResource("/libexec/" + depName);
                if (url == null) {
                    throw new RuntimeException(String.format("/libexec/%s could not be found.", depName));
                }

                atomicCopy(url, f, true);
            }
            TimeProfiler.stop();
        }
        TimeProfiler.stop();
    }

    // https://stackoverflow.com/a/30755071/468516
    private static final String ENOTEMPTY = "Directory not empty";
    private static void move(final File source, final File target) throws FileAlreadyExistsException, IOException {
        try {
            Files.move(source.toPath(), target.toPath(), StandardCopyOption.ATOMIC_MOVE);

        } catch (AccessDeniedException e) {
            // directory move collision on Windows
            throw new FileAlreadyExistsException(source.toString(), target.toString(), e.getMessage());

        } catch (FileSystemException e) {
            if (ENOTEMPTY.equals(e.getReason())) {
                // directory move collision on Unix
                throw new FileAlreadyExistsException(source.toString(), target.toString(), e.getMessage());
            } else {
                // other problem
                throw e;
            }
        }
    }

    private static void atomicCopy(URL source, File target, boolean executable) throws IOException {
        if (target.exists()) {
            return;
        }

        long t = System.nanoTime();
        File tmp = new File(target.getParent(), String.format("%s_%d", target.getName(), t));
        FileUtils.copyURLToFile(source, tmp);
        tmp.setExecutable(executable);

        try {
            move(tmp, target);
        } catch (FileAlreadyExistsException e) {
            // pass
            tmp.delete();
        }
    }

    public static String getExeWithExtension(Platform platform, String name, String extension) throws IOException {
        init();
        TimeProfiler.startF("getExeWithExtension %s.%s", name, extension);
        String exeName = platform.getPair() + "/" + platform.getExePrefix() + name + extension;
        File f = new File(rootFolder, exeName);
        if (!f.exists()) {
            URL url = Bob.class.getResource("/libexec/" + exeName);
            if (url == null) {
                throw new RuntimeException(String.format("/libexec/%s could not be found locally, create an application manifest to build the engine remotely.", exeName));
            }

            atomicCopy(url, f, true);
        }
        TimeProfiler.addData("path", f.getAbsolutePath());
        TimeProfiler.stop();
        return f.getAbsolutePath();
    }

    public static String getLibExecPath(String filename) throws IOException {
        init();
        TimeProfiler.startF("getLibExecPath %s", filename);
        File f = new File(rootFolder, filename);
        if (!f.exists()) {
            URL url = Bob.class.getResource("/libexec/" + filename);
            if (url == null) {
                throw new RuntimeException(String.format("/libexec/%s not found", filename));
            }

            atomicCopy(url, f, false);
        }
        TimeProfiler.addData("path", f.getAbsolutePath());
        TimeProfiler.stop();
        return f.getAbsolutePath();
    }

    public static String getJarFile(String filename) throws IOException {
        init();
        TimeProfiler.startF("getJarFile %s", filename);
        File f = new File(rootFolder, filename);
        if (!f.exists()) {
            URL url = Bob.class.getResource("/share/java/" + filename);
            if (url == null) {
                throw new RuntimeException(String.format("/share/java/%s not found", filename));
            }
            atomicCopy(url, f, false);
        }
        TimeProfiler.addData("path", f.getAbsolutePath());
        TimeProfiler.stop();
        return f.getAbsolutePath();
    }

    public static String getDefaultDmengineExeName(String variant) {
        switch (variant)
        {
            case VARIANT_DEBUG:
                return "dmengine";
            case VARIANT_RELEASE:
                return "dmengine_release";
            case VARIANT_HEADLESS:
                return "dmengine_headless";
            default:
                throw new RuntimeException(String.format("Invalid variant %s", variant));
        }
    }

    public static List<String> getDefaultDmenginePaths(Platform platform, String variant) throws IOException {
        return getExes(platform, getDefaultDmengineExeName(variant));
    }

    public static List<File> getDefaultDmengineFiles(Platform platform, String variant) throws IOException {
        List<String> binaryPaths = getDefaultDmenginePaths(platform, variant);
        List<File> binaryFiles = new ArrayList<File>();
        for (String path : binaryPaths) {
            binaryFiles.add(new File(path));
        }
        return binaryFiles;
    }

    public static List<File> getNativeExtensionEngineBinaries(Platform platform, String extenderExeDir) throws IOException
    {
        List<String> binaryNames = platform.formatBinaryName("dmengine");
        List<File> binaryFiles = new ArrayList<File>();
        for (String binaryName : binaryNames) {
            File extenderExe = new File(FilenameUtils.concat(extenderExeDir, FilenameUtils.concat(platform.getExtenderPair(), binaryName)));

            // All binaries must exist, otherwise return null
            if (!extenderExe.exists()) {
                return null;
            }
            binaryFiles.add(extenderExe);
        }
        return binaryFiles;
    }

    public static String getLib(Platform platform, String name) throws IOException {
        init();

        TimeProfiler.startF("getLib %s", name);
        String libName = platform.getPair() + "/" + platform.getLibPrefix() + name + platform.getLibSuffix();
        File f = new File(rootFolder, libName);
        if (!f.exists()) {
            URL url = Bob.class.getResource("/lib/" + libName);
            if (url == null) {
                throw new RuntimeException(String.format("/lib/%s not found", libName));
            }

            atomicCopy(url, f, true);
        }
        TimeProfiler.addData("path", f.getAbsolutePath());
        TimeProfiler.stop();
        return f.getAbsolutePath();
    }

    private static void addOption(Options options, String shortOpt, String longOpt, boolean hasArg, String description, boolean usedByResourceCacheKey) {
        options.addOption(shortOpt, longOpt, hasArg, description);
        if (usedByResourceCacheKey) {
            ResourceCacheKey.includeOption(longOpt);
        }
    }

    private static Options getCommandLineOptions() {
        Options options = new Options();
        addOption(options, "r", "root", true, "Build root directory. Default is current directory", true);
        addOption(options, "o", "output", true, "Output directory. Default is \"build/default\"", false);
        addOption(options, "i", "input", true, "Source directory. Default is current directory", true);
        addOption(options, "v", "verbose", false, "Verbose output", false);
        addOption(options, "h", "help", false, "This help message", false);
        addOption(options, "a", "archive", false, "Build archive", false);
        addOption(options, "e", "email", true, "User email", false);
        addOption(options, "u", "auth", true, "User auth token", false);

        addOption(options, "p", "platform", true, "Platform (when building and bundling)", true);
        addOption(options, "bo", "bundle-output", true, "Bundle output directory", false);
        addOption(options, "bf", "bundle-format", true, "Which formats to create the application bundle in. Comma separated list. (Android: 'apk' and 'aab')", false);

        addOption(options, "mp", "mobileprovisioning", true, "mobileprovisioning profile (iOS)", false);
        addOption(options, null, "identity", true, "Sign identity (iOS)", false);

        addOption(options, "ce", "certificate", true, "DEPRECATED! Use --keystore instead", false);
        addOption(options, "pk", "private-key", true, "DEPRECATED! Use --keystore instead", false);

        addOption(options, "ks", "keystore", true, "Deployment keystore used to sign APKs (Android)", false);
        addOption(options, "ksp", "keystore-pass", true, "Password of the deployment keystore (Android)", false);
        addOption(options, "ksa", "keystore-alias", true, "The alias of the signing key+cert you want to use (Android)", false);
        addOption(options, "kp", "key-pass", true, "Password of the deployment key if different from the keystore password (Android)", false);

        addOption(options, "d", "debug", false, "DEPRECATED! Use --variant=debug instead", false);
        addOption(options, null, "variant", true, "Specify debug, release or headless version of dmengine (when bundling)", false);
        addOption(options, null, "strip-executable", false, "Strip the dmengine of debug symbols (when bundling iOS or Android)", false);
        addOption(options, null, "with-symbols", false, "Generate the symbol file (if applicable)", false);

        addOption(options, "tp", "texture-profiles", true, "DEPRECATED! Use --texture-compression instead", true);
        addOption(options, "tc", "texture-compression", true, "Use texture compression as specified in texture profiles", true);
        addOption(options, "k", "keep-unused", false, "Keep unused resources in archived output", true);

        addOption(options, null, "exclude-build-folder", true, "Comma separated list of folders to exclude from the build", true);

        addOption(options, "br", "build-report", true, "Filepath where to save a build report as JSON", false);
        addOption(options, "brhtml", "build-report-html", true, "Filepath where to save a build report as HTML", false);

        addOption(options, null, "build-server", true, "The build server (when using native extensions)", true);
        addOption(options, null, "use-async-build-server", false, "Use an async build process for the build server (when using native extensions)", true);
        addOption(options, null, "defoldsdk", true, "What version of the defold sdk (sha1) to use", true);
        addOption(options, null, "binary-output", true, "Location where built engine binary will be placed. Default is \"<build-output>/<platform>/\"", true);

        addOption(options, null, "use-vanilla-lua", false, "DEPRECATED! Use --use-uncompressed-lua-source instead.", true);
        addOption(options, null, "use-uncompressed-lua-source", false, "Use uncompressed and unencrypted Lua source code instead of byte code", true);
        addOption(options, null, "archive-resource-padding", true, "The alignment of the resources in the game archive. Default is 4", true);

        addOption(options, "l", "liveupdate", true, "Yes if liveupdate content should be published", true);

        addOption(options, "ar", "architectures", true, "Comma separated list of architectures to include for the platform", false);

        addOption(options, null, "settings", true, "Path to a game project settings file. More than one occurrance are allowed. The settings files are applied left to right.", false);

        addOption(options, null, "version", false, "Prints the version number to the output", false);

        addOption(options, null, "build-artifacts", true, "If left out, will default to build the engine. Choices: 'engine', 'plugins'. Comma separated list.", false);

        addOption(options, null, "resource-cache-local", true, "Path to local resource cache.", false);
        addOption(options, null, "resource-cache-remote", true, "URL to remote resource cache.", false);
        addOption(options, null, "resource-cache-remote-user", true, "Username to authenticate access to the remote resource cache.", false);
        addOption(options, null, "resource-cache-remote-pass", true, "Password/token to authenticate access to the remote resource cache.", false);

        addOption(options, null, "manifest-private-key", true, "Private key to use when signing manifest and archive.", false);
        addOption(options, null, "manifest-public-key", true, "Public key to use when signing manifest and archive.", false);

        // debug options
        addOption(options, null, "debug-ne-upload", false, "Outputs the files sent to build server as upload.zip", false);

        return options;
    }

    private static CommandLine parse(String[] args) {
        Options options = getCommandLineOptions();

        CommandLineParser parser = new PosixParser();
        CommandLine cmd = null;
        try {
            cmd = parser.parse(options, args);
        } catch (ParseException e) {
            System.err.println(e.getMessage());
            System.exit(5);
        }
        if (cmd.hasOption("h")) {
            HelpFormatter helpFormatter = new HelpFormatter( );
            helpFormatter.printHelp("bob [options] [commands]", options);
            System.exit(0);
        }
        if (cmd.hasOption("ce") || cmd.hasOption("pk")) {
            System.out.println("Android signing using certificate and private key is no longer supported. You must use keystore signing.");
            HelpFormatter helpFormatter = new HelpFormatter( );
            helpFormatter.printHelp("bob [options] [commands]", options);
            System.exit(1);
        }

        return cmd;
    }

    private static Project createProject(String rootDirectory, String buildDirectory, String email, String auth) {
        Project project = new Project(new DefaultFileSystem(), rootDirectory, buildDirectory);
        project.setOption("email", email);
        project.setOption("auth", auth);

        return project;
    }

    public static String logExceptionToString(int severity, IResource res, int line, String message)
    {
        String resourceString = "unspecified";
        if (res != null) {
            resourceString = res.toString();
        }
        String strSeverity = "ERROR";
        if (severity == MultipleCompileException.Info.SEVERITY_INFO)
            strSeverity = "INFO";
        else if (severity == MultipleCompileException.Info.SEVERITY_WARNING)
            strSeverity = "WARNING";
        return String.format("%s: %s:%d: '%s'\n", strSeverity, resourceString, line, message);
    }

    private static void setupProject(Project project, boolean resolveLibraries, String sourceDirectory) throws IOException, LibraryException, CompileExceptionError {
        BobProjectProperties projectProperties = project.getProjectProperties();
        String[] dependencies = projectProperties.getStringArrayValue("project", "dependencies");
        List<URL> libUrls = new ArrayList<>();
        for (String val : dependencies) {
            libUrls.add(new URL(val));
        }

        project.setLibUrls(libUrls);
        if (resolveLibraries) {
            TimeProfiler.start("Resolve libs");
            project.resolveLibUrls(new ConsoleProgress());
            TimeProfiler.stop();
        }
        project.mount(new ClassLoaderResourceScanner());

        Set<String> skipDirs = new HashSet<String>(Arrays.asList(".git", project.getBuildDirectory(), ".internal"));
        project.findSources(sourceDirectory, skipDirs);
    }

    private static void validateChoices(String optionName, String value, List<String> validChoices) {
        if (!validChoices.contains(value)) {
            System.out.printf("%s option must be one of: ", optionName);
            for (String choice : validChoices) {
                System.out.printf("%s, ", choice);
            }
            System.out.printf("\n");
            System.exit(1);
        }
    }

    private static void validateChoicesList(Project project, String optionName, String[] validChoices) {
        String str = project.option(optionName, "");
        List<String> values = Arrays.asList(str.split(","));
        for (String value : values) {
            validateChoices(optionName, value, Arrays.asList(validChoices));
        }
    }

    public static boolean isPowerOfTwo(int x)
    {
        // First x in the below expression is
        // for the case when x is 0
        return x != 0 && ((x & (x - 1)) == 0);
    }

    private static void mainInternal(String[] args) throws IOException, CompileExceptionError, URISyntaxException, LibraryException {
        System.setProperty("java.awt.headless", "true");
        System.setProperty("file.encoding", "UTF-8");
        String cwd = new File(".").getAbsolutePath();

        CommandLine cmd = parse(args);
        String buildDirectory = getOptionsValue(cmd, 'o', "build/default");
        String rootDirectory = getOptionsValue(cmd, 'r', cwd);
        String sourceDirectory = getOptionsValue(cmd, 'i', ".");
        verbose = cmd.hasOption('v');

        if (cmd.hasOption("build-report") || cmd.hasOption("build-report-html")) {
            String path = cmd.getOptionValue("build-report");
            TimeProfiler.ReportFormat format = TimeProfiler.ReportFormat.JSON;
            if (path == null) {
                path = cmd.getOptionValue("build-report-html");
                format = TimeProfiler.ReportFormat.HTML;
            }
            File report = new File(path);
            TimeProfiler.init(report, format, false);
        }

        if (cmd.hasOption("version")) {
            System.out.println(String.format("bob.jar version: %s  sha1: %s  built: %s", EngineVersion.version, EngineVersion.sha1, EngineVersion.timestamp));
            System.exit(0);
            return;
        }

        if (cmd.hasOption("debug") && cmd.hasOption("variant")) {
            System.out.println("-d (--debug) option is deprecated and can't be set together with option --variant");
            System.exit(1);
            return;
        }

        if (cmd.hasOption("debug") && cmd.hasOption("strip-executable")) {
            System.out.println("-d (--debug) option is deprecated and can't be set together with option --strip-executable");
            System.exit(1);
            return;
        }

        String[] commands = cmd.getArgs();
        if (commands.length == 0) {
            commands = new String[] { "build" };
        }

        boolean shouldResolveLibs = false;
        for (String command : commands) {
            if (command.equals("resolve")) {
                shouldResolveLibs = true;
                break;
            }
        }

        String email = getOptionsValue(cmd, 'e', null);
        String auth = getOptionsValue(cmd, 'u', null);
        Project project = createProject(rootDirectory, buildDirectory, email, auth);

        if (cmd.hasOption("settings")) {
            for (String filepath : cmd.getOptionValues("settings")) {
                project.addPropertyFile(filepath);
            }
        }
        project.loadProjectFile();

        // resolves libraries and finds all sources
        setupProject(project, shouldResolveLibs, sourceDirectory);

        if (!cmd.hasOption("defoldsdk")) {
            project.setOption("defoldsdk", EngineVersion.sha1);
        }

        if (cmd.hasOption("use-vanilla-lua")) {
            System.out.println("--use-vanilla-lua option is deprecated. Use --use-uncompressed-lua-source instead.");
            project.setOption("use-uncompressed-lua-source", "true");
        }

        Option[] options = cmd.getOptions();
        for (Option o : options) {
            if (cmd.hasOption(o.getLongOpt())) {
                if (o.hasArg()) {
                    project.setOption(o.getLongOpt(), cmd.getOptionValue(o.getLongOpt()));
                } else {
                    project.setOption(o.getLongOpt(), "true");
                }
            }
        }

        // Get and set architectures list.
        Platform platform = project.getPlatform();
        String[] architectures = platform.getArchitectures().getDefaultArchitectures();
        List<String> availableArchitectures = Arrays.asList(platform.getArchitectures().getArchitectures());

        if (cmd.hasOption("architectures")) {
            architectures = cmd.getOptionValue("architectures").split(",");
        }

        if (architectures.length == 0) {
            System.out.println(String.format("ERROR! --architectures cannot be empty. Available architectures: %s", String.join(", ", availableArchitectures)));
            System.exit(1);
            return;
        }

        // Remove duplicates and make sure they are all supported for
        // selected platform.
        Set<String> uniqueArchitectures = new HashSet<String>();
        for (int i = 0; i < architectures.length; i++) {
            String architecture = architectures[i];
            if (!availableArchitectures.contains(architecture)) {
                System.out.println(String.format("ERROR! %s is not a supported architecture for %s platform. Available architectures: %s", architecture, platform.getPair(), String.join(", ", availableArchitectures)));
                System.exit(1);
                return;
            }
            uniqueArchitectures.add(architecture);
        }

        project.setOption("architectures", String.join(",", uniqueArchitectures));

        boolean shouldPublish = getOptionsValue(cmd, 'l', "no").equals("yes");
        project.setOption("liveupdate", shouldPublish ? "true" : "false");

        if (!cmd.hasOption("variant")) {
            if (cmd.hasOption("debug")) {
                System.out.println("WARNING option 'debug' is deprecated, use options 'variant' and 'strip-executable' instead.");
                project.setOption("variant", VARIANT_DEBUG);
            } else {
                project.setOption("variant", VARIANT_RELEASE);
                project.setOption("strip-executable", "true");
            }
        }

        String variant = project.option("variant", VARIANT_RELEASE);
        if (! (variant.equals(VARIANT_DEBUG) || variant.equals(VARIANT_RELEASE) || variant.equals(VARIANT_HEADLESS)) ) {
            System.out.println(String.format("--variant option must be one of %s, %s, or %s", VARIANT_DEBUG, VARIANT_RELEASE, VARIANT_HEADLESS));
            System.exit(1);
            return;
        }

        if (cmd.hasOption("texture-profiles")) {
            // If user tries to set (deprecated) texture-profiles, warn user and set texture-compression instead
            System.out.println("WARNING option 'texture-profiles' is deprecated, use option 'texture-compression' instead.");
            String texCompression = cmd.getOptionValue("texture-profiles");
            if (cmd.hasOption("texture-compression")) {
                texCompression = cmd.getOptionValue("texture-compression");
            }
            project.setOption("texture-compression", texCompression);
        }

        if (cmd.hasOption("archive-resource-padding")) {
            String resourcePaddingStr = cmd.getOptionValue("archive-resource-padding");
            int resourcePadding = 0;
            try {
                resourcePadding = Integer.parseInt(resourcePaddingStr);
            } catch (Exception e) {
                System.out.printf("Could not parse --archive-resource-padding='%s' into a valid integer\n", resourcePaddingStr);
                System.exit(1);
                return;
            }

            if (!isPowerOfTwo(resourcePadding)) {
                System.out.printf("Argument --archive-resource-padding='%s' isn't a power of two\n", resourcePaddingStr);
                System.exit(1);
                return;
            }

            project.setOption("archive-resource-padding", resourcePaddingStr);
        }

        if (project.hasOption("build-artifacts")) {
            String[] validArtifacts = {"engine", "plugins"};
            validateChoicesList(project, "build-artifacts", validArtifacts);
        }

        boolean ret = true;
        StringBuilder errors = new StringBuilder();

        List<TaskResult> result = new ArrayList<>();
        try {
            result = project.build(new ConsoleProgress(), commands);
        } catch(MultipleCompileException e) {
            ret = false;
            errors.append("\n");
            for (MultipleCompileException.Info info : e.issues)
            {
                errors.append(logExceptionToString(info.getSeverity(), info.getResource(), info.getLineNumber(), info.getMessage()) + "\n");
            }

            String msg = String.format("For the full log, see %s (or add -v)\n", e.getLogPath());
            errors.append(msg);

            if (verbose) {
                errors.append("\nFull log: \n" + e.getRawLog() + "\n");
            }
        }
        for (TaskResult taskResult : result) {
            if (!taskResult.isOk()) {
                ret = false;
                String message = taskResult.getMessage();
                if (message == null || message.isEmpty()) {
                    if (taskResult.getException() != null) {
                        message = taskResult.getException().getMessage();
                    } else {
                        message = "undefined";
                    }
                }
                errors.append(String.format("ERROR %s%s %s\n", taskResult.getTask().getInputs().get(0),
                        (taskResult.getLineNumber() != -1) ? String.format(":%d", taskResult.getLineNumber()) : "",
                        message));
                if (verbose) {
                    if (taskResult.getException() != null) {
                        errors.append("  ")
                                .append(taskResult.getException().toString())
                                .append("\n");
                        StackTraceElement[] elements = taskResult
                                .getException().getStackTrace();
                        for (StackTraceElement element : elements) {
                            errors.append("  ").append(element.toString())
                                    .append("\n");
                        }
                    }
                }
            }
        }
        if (!ret) {
            System.out.println("\nThe build failed for the following reasons:");
            System.out.println(errors.toString());
        }
        project.dispose();
        System.exit(ret ? 0 : 1);
    }

    private static void logErrorAndExit(Exception e) {
        System.err.println(e.getMessage());
        if (e.getCause() != null) {
            System.err.println("Cause: " + e.getCause());
        }
        if (verbose) {
            e.printStackTrace();
        }
        System.exit(1);
    }

    public static void main(String[] args) throws IOException, CompileExceptionError, URISyntaxException, LibraryException {
        try {
            mainInternal(args);
        } catch (LibraryException|CompileExceptionError e) {
            logErrorAndExit(e);
        } catch (Exception e) {
            logErrorAndExit(e);
        }

    }

    private static String getOptionsValue(CommandLine cmd, char o, String defaultValue) {
        String value = defaultValue;

        if (cmd.hasOption(o)) {
            value = cmd.getOptionValue(o);
        }
        return value;
    }

    public static void verbose(String message, Object... args) {
        if (verbose) {
            System.out.println("Bob: " + String.format(message, args));
        }
    }

}
