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
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.lang.NumberFormatException;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.Option;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.archive.EngineVersion;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.logging.LogHelper;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.TimeProfiler;
import com.dynamo.bob.util.HttpUtil;
import com.dynamo.bob.cache.ResourceCacheKey;
import com.dynamo.bob.util.FileUtil;

import static com.dynamo.bob.Bob.CommandLineOption.ArgCount.*;
import static com.dynamo.bob.Bob.CommandLineOption.ArgType.ABS_OR_CWD_REL_PATH;

public class Bob {
    private static final Logger logger = Logger.getLogger(Bob.class.getName());

    public static final String VARIANT_DEBUG = "debug";
    public static final String VARIANT_RELEASE = "release";
    public static final String VARIANT_HEADLESS = "headless";

    public static final String ARTIFACTS_URL = "http://d.defold.com/archive/";

    private static File rootFolder = null;
    private static boolean luaInitialized = false;

    public static class OptionValidationException extends Exception {
        public final int exitCode;

        public OptionValidationException(int exitCode) {
            this.exitCode = exitCode;
        }
    }

    public static class InvocationResult {
        public final boolean success;
        public final List<TaskResult> taskResults;

        public InvocationResult(boolean success, List<TaskResult> taskResults) {
            this.success = success;
            this.taskResults = taskResults;
        }
    }

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

    public static void init() {
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

    public static File getRootFolder() {
        return rootFolder;
    }

    public static void extractToFolder(final URL url, File toFolder, boolean deleteOnExit) throws IOException {
        TimeProfiler.startF("extractToFolder %s", toFolder.toString());
        TimeProfiler.addData("url", url.toString());
        ZipInputStream zipStream = new ZipInputStream(new BufferedInputStream(url.openStream()));

        try{
            ZipEntry entry = zipStream.getNextEntry();
            while (entry != null)
            {
                if (!entry.isDirectory()) {

                    File dstFile = new File(toFolder, entry.getName());
                    if (deleteOnExit)
                        FileUtil.deleteOnExit(dstFile);
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
                    logger.fine("Extracted '%s' from '%s' to '%s'", entry.getName(), url, dstFile.getAbsolutePath());
                }

                entry = zipStream.getNextEntry();
            }
        } finally {
            IOUtils.closeQuietly(zipStream);
        }
        TimeProfiler.stop();
    }

    public static void extract(final URL url, File toFolder) throws IOException {
        extractToFolder(url, toFolder, true);
    }

    public static String getPath(String path) {
        init();
        File f = new File(rootFolder, path);
        if (!f.exists()) {
            throw new RuntimeException(String.format("location %s not found", f.toString()));
        }
        return f.getAbsolutePath();
    }

    private static List<String> getExes(Platform platform, String name) throws IOException {
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

    public static void atomicCopy(URL source, File target, boolean executable) throws IOException {
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

    private static String getExeWithExtension(Platform platform, String name, String extension) throws IOException {
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

    private static List<File> downloadExes(Platform platform, String variant, String artifactsURL) throws IOException {
        init();
        TimeProfiler.startF("DownloadExes %s for %s", platform, variant);
        List<File> binaryFiles = new ArrayList<File>();
        String[] exeSuffixes = platform.getExeSuffixes();
        List<String> exes = new ArrayList<String>();
        String downloadFolder = rootFolder + File.pathSeparator + platform.getPair() + File.separator + platform;
        String defaultDmengineExeName = getDefaultDmengineExeName(variant);
        for (String exeSuffix : exeSuffixes) {
            String exeName = platform.getExePrefix() + defaultDmengineExeName + exeSuffix;
            File f = new File(rootFolder, exeName);
            try {
                URL url = new URL(String.format(artifactsURL + "%s/engine/%s/%s", EngineVersion.sha1, platform.getPair(), exeName));
                logger.info("Download: %s", url);
                File file = new File(downloadFolder, exeName);
                HttpUtil http = new HttpUtil();
                http.downloadToFile(url, file);
                FileUtil.deleteOnExit(file);
                binaryFiles.add(file);
            }
            catch (Exception e) {
                throw new IOException(String.format("%s could not be found locally or downloaded, create an application manifest to build the engine remotely.", exeName));
            }
        }
        TimeProfiler.stop();
        return binaryFiles;
    }

    private static String getDefaultDmengineExeName(String variant) {
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

    private static List<String> getDefaultDmenginePaths(Platform platform, String variant) throws IOException {
        return getExes(platform, getDefaultDmengineExeName(variant));
    }

    public static List<File> getDefaultDmengineFiles(Platform platform, String variant, String artifactsURL) throws IOException {
        List<File> binaryFiles;
        try {
            List<String> binaryPaths = getDefaultDmenginePaths(platform, variant);
            binaryFiles = new ArrayList<File>();
            for (String path : binaryPaths) {
                binaryFiles.add(new File(path));
            }
        }
        catch (RuntimeException e) {
            binaryFiles = downloadExes(platform, variant, artifactsURL);
        }
        return binaryFiles;
    }

    public static List<File> getDefaultDmengineFiles(Platform platform, String variant) throws IOException {
        return getDefaultDmengineFiles(platform, variant, ARTIFACTS_URL);
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

    public static File getSharedLib(String name) throws IOException {
        init();
        TimeProfiler.startF("getSharedLib %s", name);
        Platform platform = Platform.getHostPlatform();
        String libName = platform.getPair() + "/" + platform.getLibPrefix() + name + platform.getLibSuffix();
        File f = new File(rootFolder, libName);
        if (!f.exists()) {
            URL url = Bob.class.getResource("/lib/" + libName);
            if (url == null) {
                throw new RuntimeException(String.format("/lib/%s not found", libName));
            }

            atomicCopy(url, f, true);
        }
        TimeProfiler.stop();
        return f;
    }

    static void addToPath(String variable, String path) {
        String newPath = null;

        // Check if jna.library.path is set externally.
        if (System.getProperty(variable) != null) {
            newPath = System.getProperty(variable);
        }

        if (newPath == null) {
            // Set path where model_shared library is found.
            newPath = path;
        } else {
            // Append path where model_shared library is found.
            newPath += File.pathSeparator + path;
        }

        // Set the concatenated jna.library path
        System.setProperty(variable, newPath);
    }

    static public void addToPaths(String dir) {
        addToPath("jni.library.path", dir);
        addToPath("jna.library.path", dir);
        addToPath("java.library.path", dir);
    }

    public static final class CommandLineOption {
        public final String shortOpt;
        public final String longOpt;
        public final ArgCount argCount;
        public final ArgType argType;
        public final String description;

        public CommandLineOption(String shortOpt, String longOpt, ArgCount argCount, ArgType argType, String description) {
            this.shortOpt = shortOpt;
            this.longOpt = longOpt;
            this.argCount = argCount;
            this.argType = argType;
            this.description = description;
        }

        public enum ArgCount {ZERO, ONE, MANY}

        public enum ArgType {UNSPECIFIED, ABS_OR_CWD_REL_PATH}
    }

    private static CommandLineOption opt(String shortOpt, String longOpt, CommandLineOption.ArgCount argCount, CommandLineOption.ArgType argType, String description,
                                         boolean usedByResourceCacheKey) {
        if (usedByResourceCacheKey) {
            ResourceCacheKey.includeOption(longOpt);
        }
        if (argCount == MANY) {
            description = description + ". More than one occurrence is allowed";
        }
        return new CommandLineOption(shortOpt, longOpt, argCount, argType, description);
    };

    private static CommandLineOption opt(String shortOpt, String longOpt, CommandLineOption.ArgCount argCount, String description, boolean usedByResourceCacheKey) {
        return opt(shortOpt, longOpt, argCount, CommandLineOption.ArgType.UNSPECIFIED, description, usedByResourceCacheKey);
    }

    public static List<CommandLineOption> getCommandLineOptions() {
        return List.of(
                opt("r", "root", ONE, ABS_OR_CWD_REL_PATH, "Build root directory. Default is current directory", true),
                opt("o", "output", ONE, "Output directory. Default is \"build/default\"", false),
                opt("i", "input", ONE, "DEPRECATED! Use --root instead", true),
                opt("v", "verbose", ZERO, "Verbose output", false),
                opt("h", "help", ZERO, "This help message", false),
                opt("a", "archive", ZERO, "Build archive", false),
                opt("ea", "exclude-archive", ZERO, "Exclude resource archives from application bundle. Use this to create an empty Defold application for use as a build target", false),
                opt("e", "email", ONE, "User email", false),
                opt("u", "auth", ONE, "User auth token", false),

                opt("p", "platform", ONE, "Platform (when building and bundling)", true),
                opt("bo", "bundle-output", ONE, ABS_OR_CWD_REL_PATH,"Bundle output directory", false),
                opt("bf", "bundle-format", ONE, "Which formats to create the application bundle in. Comma separated list. (Android: 'apk' and 'aab')", false),

                opt("mp", "mobileprovisioning", ONE, ABS_OR_CWD_REL_PATH, "mobileprovisioning profile (iOS)", false),
                opt(null, "identity", ONE, "Sign identity (iOS)", false),

                opt("ce", "certificate", ONE, "DEPRECATED! Use --keystore instead", false),
                opt("pk", "private-key", ONE, "DEPRECATED! Use --keystore instead", false),

                opt("ks", "keystore", ONE, "Deployment keystore used to sign APKs (Android)", false),
                opt("ksp", "keystore-pass", ONE, "Password of the deployment keystore (Android)", false),
                opt("ksa", "keystore-alias", ONE, "The alias of the signing key+cert you want to use (Android)", false),
                opt("kp", "key-pass", ONE, "Password of the deployment key if different from the keystore password (Android)", false),

                opt("d", "debug", ZERO, "DEPRECATED! Use --variant=debug instead", false),
                opt(null, "variant", ONE, "Specify debug, release or headless version of dmengine (when bundling)", false),
                opt(null, "strip-executable", ZERO, "Strip the dmengine of debug symbols (when bundling iOS or Android)", false),
                opt(null, "with-symbols", ZERO, "Generate the symbol file (if applicable)", false),

                opt("tp", "texture-profiles", ONE, "DEPRECATED! Use --texture-compression instead", true),
                opt("tc", "texture-compression", ONE, "Use texture compression as specified in texture profiles", true),

                opt(null, "exclude-build-folder", ONE, "DEPRECATED! Use '.defignore' file instead", true),

                opt("br", "build-report", ONE, ABS_OR_CWD_REL_PATH, "DEPRECATED! Use --build-report-json instead", false),
                opt("brjson", "build-report-json", ONE, ABS_OR_CWD_REL_PATH, "Filepath where to save a build report as JSON", false),
                opt("brhtml", "build-report-html", ONE, ABS_OR_CWD_REL_PATH, "Filepath where to save a build report as HTML", false),

                opt(null, "build-server", ONE, "The build server (when using native extensions)", true),
                opt(null, "build-server-header", MANY, "Additional build server header to set", true),
                opt(null, "use-async-build-server", ZERO, "DEPRECATED! Asynchronous build is now the default", true),
                opt(null, "defoldsdk", ONE, "What version of the defold sdk (sha1) to use", true),
                opt(null, "binary-output", ONE, ABS_OR_CWD_REL_PATH, "Location where built engine binary will be placed. Default is \"<build-output>/<platform>/\"", true),

                opt(null, "use-vanilla-lua", ZERO, "DEPRECATED! Use --use-uncompressed-lua-source instead", true),
                opt(null, "use-uncompressed-lua-source", ZERO, "Use uncompressed and unencrypted Lua source code instead of byte code", true),
                opt(null, "use-lua-bytecode-delta", ZERO, "Use byte code delta compression when building for multiple architectures", true),
                opt(null, "archive-resource-padding", ONE, "The alignment of the resources in the game archive. Default is 4", true),

                opt("l", "liveupdate", ONE, "Yes if liveupdate content should be published", true),

                opt("ar", "architectures", ONE, "Comma separated list of architectures to include for the platform", true),

                opt(null, "settings", MANY, ABS_OR_CWD_REL_PATH, "Path to a game project settings file. The settings files are applied left to right", false),

                opt(null, "version", ZERO, "Prints the version number to the output", false),

                opt(null, "build-artifacts", ONE, "If left out, will default to build the engine. Choices: 'engine', 'plugins', 'library'. Comma separated list", false),
                opt(null, "ne-build-dir", MANY, ABS_OR_CWD_REL_PATH, "Specify a folder with includes or source, to build a specific library", false),
                opt(null, "ne-output-name", ONE, "Specify a library target name", false),

                opt(null, "resource-cache-local", ONE, ABS_OR_CWD_REL_PATH, "Path to local resource cache", false),
                opt(null, "resource-cache-remote", ONE, "URL to remote resource cache", false),
                opt(null, "resource-cache-remote-user", ONE, "Username to authenticate access to the remote resource cache", false),
                opt(null, "resource-cache-remote-pass", ONE, "Password/token to authenticate access to the remote resource cache", false),

                opt(null, "manifest-private-key", ONE, "Private key to use when signing manifest and archive", false),
                opt(null, "manifest-public-key", ONE, "Public key to use when signing manifest and archive", false),

                opt(null, "max-cpu-threads", ONE, "Max count of threads that bob.jar can use", false),

                // debug options
                opt(null, "debug-ne-upload", ZERO, "Outputs the files sent to build server as upload.zip", false),
                opt(null, "debug-output-spirv", ONE, "Force build SPIR-V shaders", false),
                opt(null, "debug-output-wgsl", ONE, "Force build WGSL shaders", false)
        );


    }

    private static CommandLine parse(String[] args) throws OptionValidationException {
        Options options = new Options();
        getCommandLineOptions().forEach(opt -> options.addOption(opt.shortOpt, opt.longOpt, opt.argCount != ZERO, opt.description));

        CommandLineParser parser = new PosixParser();
        CommandLine cmd;
        try {
            cmd = parser.parse(options, args);
        } catch (ParseException e) {
            System.err.println(e.getMessage());
            throw new OptionValidationException(5);
        }
        if (cmd.hasOption("h")) {
            HelpFormatter helpFormatter = new HelpFormatter();
            helpFormatter.printHelp("bob [options] [commands]", options);
            return null;
        }
        if (cmd.hasOption("ce") || cmd.hasOption("pk")) {
            System.out.println("Android signing using certificate and private key is no longer supported. You must use keystore signing.");
            HelpFormatter helpFormatter = new HelpFormatter( );
            helpFormatter.printHelp("bob [options] [commands]", options);
            throw new OptionValidationException(1);
        }

        return cmd;
    }

    private static Project createProject(ClassLoader classLoader, String rootDirectory, String buildDirectory, String email, String auth) {
        Project project = new Project(classLoader, new DefaultFileSystem(), rootDirectory, buildDirectory);
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

    private static void setupProject(Project project, boolean resolveLibraries, IProgress progress) throws IOException, LibraryException, CompileExceptionError {
        BobProjectProperties projectProperties = project.getProjectProperties();
        String[] dependencies = projectProperties.getStringArrayValue("project", "dependencies");
        List<URL> libUrls = new ArrayList<>();
        for (String val : dependencies) {
            libUrls.add(new URL(val));
        }

        project.setLibUrls(libUrls);
        if (resolveLibraries) {
            TimeProfiler.start("Resolve libs");
            project.resolveLibUrls(progress);
            TimeProfiler.stop();
        }
        project.mount(new ClassLoaderResourceScanner());
    }

    private static void validateChoices(String optionName, String value, List<String> validChoices) throws OptionValidationException {
        if (!validChoices.contains(value)) {
            System.out.printf("%s option must be one of: ", optionName);
            for (String choice : validChoices) {
                System.out.printf("%s, ", choice);
            }
            System.out.printf("\n");
            throw new OptionValidationException(1);
        }
    }

    private static void validateChoicesList(Project project, String optionName, String[] validChoices) throws OptionValidationException {
        String str = project.option(optionName, "");
        String[] values = str.split(",");
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

    private static StringBuilder parseMultipleException(MultipleCompileException e, boolean verbose) {
        StringBuilder errors = new StringBuilder();
        errors.append("\n");
        for (MultipleCompileException.Info info : e.issues)
        {
            errors.append(logExceptionToString(info.getSeverity(), info.getResource(), info.getLineNumber(), info.getMessage())).append("\n");
        }

        String msg = String.format("For the full log, see %s (or add -v)\n", e.getLogPath());
        errors.append(msg);

        if (verbose) {
            errors.append("\nFull log: \n").append(e.getRawLog()).append("\n");
        }
        return errors;
    }

    public static boolean isCause(Class<? extends Throwable> expected, Throwable exc) {
       return expected.isInstance(exc) || (exc != null && isCause(expected, exc.getCause()));
    }

    /**
     * Common entry point for both editor and bob
     * @param classLoader may be null
     */
    public static InvocationResult invoke(ClassLoader classLoader, IProgress progress, boolean fromEditor, Map<String, String> internalOptions, String[] args) throws IOException, CompileExceptionError, LibraryException, OptionValidationException {
        System.setProperty("java.awt.headless", "true");
        System.setProperty("file.encoding", "UTF-8");

        String cwd = new File(".").getAbsolutePath();

        CommandLine cmd = parse(args);
        if (cmd == null) { // nothing to do: requested to print help
            return new InvocationResult(true, Collections.emptyList());
        }
        String buildDirectory = getOptionsValue(cmd, 'o', "build/default");
        String rootDirectory = getOptionsValue(cmd, 'r', cwd);

        String build_report_json = null;
        String build_report_html = null;
        if (cmd.hasOption("build-report")) {
            System.out.println("--build-report option is deprecated. Use --build-report-json instead.");
            String path = cmd.getOptionValue("build-report", "report.json");
            if (path.endsWith(".json")) {
                build_report_json = path;
            }
            else if (path.endsWith(".html")) {
                build_report_html = path;
            }
        }
        build_report_json = build_report_json != null ? build_report_json : cmd.getOptionValue("build-report-json");
        build_report_html = build_report_html != null ? build_report_html : cmd.getOptionValue("build-report-html");

        if (build_report_json != null || build_report_html != null) {
            List<File> reportFiles = new ArrayList<>();
            if (build_report_json != null) {
                reportFiles.add(new File(build_report_json));
            }
            if (build_report_html != null) {
                reportFiles.add(new File(build_report_html));
            }
            TimeProfiler.init(reportFiles, fromEditor);
        }

        TimeProfiler.start("ParseCommandLine");
        if (cmd.hasOption("version")) {
            System.out.println(String.format("bob.jar version: %s  sha1: %s  built: %s", EngineVersion.version, EngineVersion.sha1, EngineVersion.timestamp));
            return new InvocationResult(true, Collections.emptyList());
        }

        if (cmd.hasOption("debug") && cmd.hasOption("variant")) {
            System.out.println("-d (--debug) option is deprecated and can't be set together with option --variant");
            throw new OptionValidationException(1);
        }

        if (cmd.hasOption("debug") && cmd.hasOption("strip-executable")) {
            System.out.println("-d (--debug) option is deprecated and can't be set together with option --strip-executable");
            throw new OptionValidationException(1);
        }

        if (cmd.hasOption("exclude-build-folder")) {
            // Deprecated in 1.5.1. Just a message for now.
            System.out.println("--exclude-build-folder option is deprecated. Use '.defignore' file instead");
        }

        if (cmd.hasOption("input")) {
            System.out.println("-i (--input) option is deprecated. Use --root instead.");
            throw new OptionValidationException(1);
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

        boolean verbose = cmd.hasOption('v');

        LogHelper.setVerboseLogging(verbose);  // It doesn't iterate over all loggers (including the bob logger)
        LogHelper.configureLogger(logger);     // It was created before the log helper was set to be verbose

        String email = getOptionsValue(cmd, 'e', null);
        String auth = getOptionsValue(cmd, 'u', null);
        Project project = createProject(classLoader, rootDirectory, buildDirectory, email, auth);

        if (cmd.hasOption("settings")) {
            for (String filepath : cmd.getOptionValues("settings")) {
                project.addPropertyFile(filepath);
            }
        }

        if (cmd.hasOption("ne-build-dir")) {
            for (String filepath : cmd.getOptionValues("ne-build-dir")) {
                project.addEngineBuildDir(filepath);
            }
        }


        if (cmd.hasOption("build-server-header")) {
            for (String header : cmd.getOptionValues("build-server-header")) {
                project.addBuildServerHeader(header);
            }
        }
        TimeProfiler.stop();

        TimeProfiler.start("loadProjectFile");
        project.loadProjectFile();
        TimeProfiler.stop();

        TimeProfiler.start("setupProject");
        // resolves libraries and finds all sources
        setupProject(project, shouldResolveLibs, progress);
        TimeProfiler.stop();

        TimeProfiler.start("setOptions");
        if (!cmd.hasOption("defoldsdk")) {
            project.setOption("defoldsdk", EngineVersion.sha1);
        }

        if (cmd.hasOption("use-vanilla-lua")) {
            System.out.println("--use-vanilla-lua option is deprecated. Use --use-uncompressed-lua-source instead.");
            project.setOption("use-uncompressed-lua-source", "true");
        }

        if (cmd.hasOption("build-report")) {
            if (build_report_json != null) {
                project.setOption("build-report-json", build_report_json);
            }
            else if (build_report_html != null) {
                project.setOption("build-report-html", build_report_html);
            }
        }

        if (cmd.hasOption("max-cpu-threads")) {
            try {
                Integer.parseInt(cmd.getOptionValue("max-cpu-threads"));
            }
            catch (NumberFormatException ex) {
                System.out.println("`--max-cpu-threads` expects integer value.");
                ex.printStackTrace();
                throw new OptionValidationException(1);
            }
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
        if (internalOptions != null) {
            internalOptions.forEach(project::setOption);
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
            throw new OptionValidationException(1);
        }

        // Remove duplicates and make sure they are all supported for
        // selected platform.
        Set<String> uniqueArchitectures = new HashSet<String>();
        for (String architecture : architectures) {
            if (!availableArchitectures.contains(architecture)) {
                System.out.println(String.format("ERROR! %s is not a supported architecture for %s platform. Available architectures: %s", architecture, platform.getPair(), String.join(", ", availableArchitectures)));
                throw new OptionValidationException(1);
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
            throw new OptionValidationException(1);
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
            int resourcePadding;
            try {
                resourcePadding = Integer.parseInt(resourcePaddingStr);
            } catch (Exception e) {
                System.out.printf("Could not parse --archive-resource-padding='%s' into a valid integer\n", resourcePaddingStr);
                throw new OptionValidationException(1);
            }

            if (!isPowerOfTwo(resourcePadding)) {
                System.out.printf("Argument --archive-resource-padding='%s' isn't a power of two\n", resourcePaddingStr);
                throw new OptionValidationException(1);
            }

            project.setOption("archive-resource-padding", resourcePaddingStr);
        }

        if (project.hasOption("build-artifacts")) {
            String[] validArtifacts = {"engine", "plugins", "library"};
            validateChoicesList(project, "build-artifacts", validArtifacts);
        }
        TimeProfiler.stop();

        boolean ret = true;
        StringBuilder errors = new StringBuilder();

        List<TaskResult> result = new ArrayList<>();
        try {
            result = project.build(progress, commands);
        } catch(MultipleCompileException e) {
            errors = parseMultipleException(e, verbose);
            ret = false;
        } catch(CompileExceptionError e) {
            ret = false;
            if (isCause(MultipleCompileException.class, e)) {
                errors = parseMultipleException((MultipleCompileException)e.getCause(), verbose);
            } else {
                throw e;
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
                errors.append(String.format("ERROR %s%s %s\n", taskResult.getTask().input(0),
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
            System.out.println(errors);
        }
        project.dispose();
        return new InvocationResult(ret, result);
    }

    private static void logErrorAndExit(Exception e) {
        logger.severe(e.getMessage().toString());
        Throwable cause = e.getCause();
        if(cause != null) {
            for(int i = 0; cause != null; ++i) {
                logger.severe("Cause:%d: %s", i, cause.toString());
                StackTraceElement[] stackTrace = cause.getStackTrace();
                for (StackTraceElement element : stackTrace) {
                    logger.severe(element.toString());
                }
                cause = cause.getCause();
            }
        } else {
            StackTraceElement[] stackTrace = e.getStackTrace();
            for (StackTraceElement element : stackTrace) {
                logger.severe(element.toString());
            }
        }
        logger.severe(e.getMessage(), e);
        System.exit(1);
    }

    public static void main(String[] args) throws IOException, CompileExceptionError, URISyntaxException, LibraryException {
        try {
            boolean success = invoke(null, new ConsoleProgress(), false, null, args).success;
            System.exit(success ? 0 : 1);
        } catch (OptionValidationException e) {
            System.exit(e.exitCode);
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
}
