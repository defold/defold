package com.dynamo.bob;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.URISyntaxException;
import java.net.URL;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
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
import com.dynamo.bob.util.LibraryUtil;

public class Bob {

    private static boolean verbose = false;
    private static File rootFolder = null;

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
                    throw new RuntimeException("Failed to delete temp directory: " + tmpDirFile, e);
                }
            }
        }));
      }

    private static void init() {
        if (rootFolder != null) {
            return;
        }

        try {
            rootFolder = Files.createTempDirectory(null).toFile();

            // Make sure we remove the temp folder on exit
            registerShutdownHook();

            // Android SDK aapt is dynamically linked against libc++.so, we need to extract it so that
            // aapt will find it later when AndroidBundler is run.
            String libc_filename = Platform.getHostPlatform().getLibPrefix() + "c++" + Platform.getHostPlatform().getLibSuffix();
            URL libc_url = Bob.class.getResource("/lib/" + Platform.getHostPlatform().getPair() + "/" + libc_filename);
            if (libc_url != null) {
                FileUtils.copyURLToFile(libc_url, new File(rootFolder, Platform.getHostPlatform().getPair() + "/lib/" + libc_filename));
            }

            extract(Bob.class.getResource("/lib/android-res.zip"), rootFolder);
            extract(Bob.class.getResource("/lib/luajit-share.zip"), new File(rootFolder, "share"));

            // NOTE: android.jar and classes.dex aren't are only available in "full bob", i.e. from CI
            URL android_jar = Bob.class.getResource("/lib/android.jar");
            if (android_jar != null) {
                FileUtils.copyURLToFile(android_jar, new File(rootFolder, "lib/android.jar"));
            }
            URL classes_dex = Bob.class.getResource("/lib/classes.dex");
            if (classes_dex != null) {
                FileUtils.copyURLToFile(classes_dex, new File(rootFolder, "lib/classes.dex"));
            }

        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private static void extract(final URL url, File toFolder) throws IOException {

        ZipInputStream zipStream = new ZipInputStream(new BufferedInputStream(url.openStream()));

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
    }

    public static String getPath(String path) {
        init();
        File f = new File(rootFolder, path);
        if (!f.exists()) {
            throw new RuntimeException(String.format("location %s not found", f.toString()));
        }
        return f.getAbsolutePath();
    }

    public static String getExe(Platform platform, String name) throws IOException {
        init();

        String exeName = platform.getPair() + "/" + platform.getExePrefix() + name + platform.getExeSuffix();
        URL url = Bob.class.getResource("/libexec/" + exeName);
        if (url == null) {
            throw new RuntimeException(String.format("/libexec/%s not found", exeName));
        }
        File f = new File(rootFolder, exeName);
        if (!f.exists()) {
            FileUtils.copyURLToFile(url, f);
            f.setExecutable(true);
        }

        return f.getAbsolutePath();
    }

    public static String getLibExecPath(String filename) throws IOException {
        init();
        URL url = Bob.class.getResource("/libexec/" + filename);
        if (url == null) {
            throw new RuntimeException(String.format("/libexec/%s not found", filename));
        }
        File f = new File(rootFolder, filename);
        if (!f.exists()) {
            FileUtils.copyURLToFile(url, f);
        }
        return f.getAbsolutePath();
    }

    public static String getDmengineExeName(Platform platform, boolean debug) {
        if(debug) {
            return "dmengine";
        }
        else {
            return "dmengine_release";
        }
    }

    public static String getDmengineExe(Platform platform, boolean debug) throws IOException {
        return getExe(platform, getDmengineExeName(platform, debug));
    }

    public static String getLib(Platform platform, String name) throws IOException {
        init();

        String libName = platform.getPair() + "/" + platform.getLibPrefix() + name + platform.getLibSuffix();
        URL url = Bob.class.getResource("/lib/" + libName);
        if (url == null) {
            throw new RuntimeException(String.format("/lib/%s not found", libName));
        }
        File f = new File(rootFolder, libName);
        if (!f.exists()) {
            FileUtils.copyURLToFile(url, f);
            f.setExecutable(true);
        }
        return f.getAbsolutePath();
    }

    private static CommandLine parse(String[] args) {
        Options options = new Options();
        options.addOption("r", "root", true, "Build root directory. Default is current directory");
        options.addOption("o", "output", true, "Output directory. Default is \"build/default\"");
        options.addOption("i", "input", true, "Source directory. Default is current directory");
        options.addOption("v", "verbose", false, "Verbose output");
        options.addOption("h", "help", false, "This help message");
        options.addOption("a", "archive", false, "Build archive");
        options.addOption("e", "email", true, "User email");
        options.addOption("u", "auth", true, "User auth token");

        options.addOption("p", "platform", true, "Platform (when bundling)");
        options.addOption("bo", "bundle-output", true, "Bundle output directory");

        options.addOption("mp", "mobileprovisioning", true, "mobileprovisioning profile (iOS)");
        options.addOption(null, "identity", true, "Sign identity (iOS)");

        options.addOption("ce", "certificate", true, "Certificate (Android)");
        options.addOption("pk", "private-key", true, "Private key (Android)");

        options.addOption("d", "debug", false, "Use debug version of dmengine (when bundling)");

        options.addOption("tp", "texture-profiles", true, "Use texture profiles (deprecated)");
        options.addOption("tc", "texture-compression", true, "Use texture compression as specified in texture profiles");
        options.addOption("k", "keep-unused", false, "Keep unused resources in archived output");

        options.addOption("br", "build-report", true, "Filepath where to save a build report as JSON");
        options.addOption("brhtml", "build-report-html", true, "Filepath where to save a build report as HTML");

        options.addOption(null, "build-server", true, "The build server (when using native extensions)");
        options.addOption(null, "defoldsdk", true, "What version of the defold sdk (sha1) to use");
        options.addOption(null, "binary-output", true, "Location where built engine binary will be placed. Default is \"<build-output>/<platform>/\"");

        options.addOption("l", "liveupdate", true, "yes if liveupdate content should be published");

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
        return cmd;
    }

    public static Project createProject(String sourceDirectory, String rootDirectory, String buildDirectory, boolean resolveLibraries, String email, String auth) throws IOException, LibraryException, CompileExceptionError {
        Project project = new Project(new DefaultFileSystem(), rootDirectory, buildDirectory);
        project.setOption("email", email);
        project.setOption("auth", auth);

        ClassLoaderScanner scanner = new ClassLoaderScanner();
        project.scan(scanner, "com.dynamo.bob");
        project.scan(scanner, "com.dynamo.bob.pipeline");

        String cwd = new File(".").getAbsolutePath();
        project.setLibUrls(LibraryUtil.getLibraryUrlsFromProject(FilenameUtils.concat(cwd, rootDirectory)));
        if (resolveLibraries) {
            project.resolveLibUrls(new ConsoleProgress());
        }
        project.mount(new ClassLoaderResourceScanner());

        Set<String> skipDirs = new HashSet<String>(Arrays.asList(".git", buildDirectory, ".internal"));
        project.findSources(sourceDirectory, skipDirs);

        return project;
    }

    public static void main(String[] args) throws IOException, CompileExceptionError, MultipleCompileException, URISyntaxException, LibraryException {
        System.setProperty("java.awt.headless", "true");
        String cwd = new File(".").getAbsolutePath();

        CommandLine cmd = parse(args);
        String buildDirectory = getOptionsValue(cmd, 'o', "build/default");
        String rootDirectory = getOptionsValue(cmd, 'r', cwd);
        String sourceDirectory = getOptionsValue(cmd, 'i', ".");
        verbose = cmd.hasOption('v');

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
        Project project = createProject(sourceDirectory, rootDirectory, buildDirectory, shouldResolveLibs, email, auth);
        if (!cmd.hasOption("defoldsdk")) {
            project.setOption("defoldsdk", EngineVersion.sha1);
        }

        boolean shouldPublish = getOptionsValue(cmd, 'l', "no").equals("yes");
        project.createPublisher(shouldPublish);

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

        if (cmd.hasOption("texture-profiles")) {
            // If user tries to set (deprecated) texture-profiles, warn user and set texture-compression instead
            System.out.println("WARNING option 'texture-profiles' is deprecated, setting 'texture-compression' option instead.");
            String texCompression = cmd.getOptionValue("texture-profiles");
            if (cmd.hasOption("texture-compression")) {
                texCompression = cmd.getOptionValue("texture-compression");
            }
            project.setOption("texture-compression", texCompression);
        }

        List<TaskResult> result = project.build(new ConsoleProgress(), commands);
        boolean ret = true;
        StringBuilder errors = new StringBuilder();
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
            System.out.println("The build failed for the following reasons:");
            System.out.println(errors.toString());
        }
        project.dispose();
        System.exit(ret ? 0 : 1);
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
