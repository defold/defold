package com.dynamo.bob;

import java.io.Closeable;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.security.CodeSource;
import java.security.ProtectionDomain;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.UUID;
import java.util.zip.ZipEntry;
import java.util.zip.ZipException;
import java.util.zip.ZipFile;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.util.LibraryUtil;

public class Bob {

    private static String texcLibDir = null;
    private static boolean verbose = false;

    private static CommandLine parse(String[] args) {
        Options options = new Options();
        options.addOption("r", "root", true, "Build root directory. Default is current directory");
        options.addOption("o", "out", true, "Output directory. Default is \"build/default\"");
        options.addOption("i", "input", true, "Source directory. Default is current directory");
        options.addOption("v", "verbose", false, "Verbose output");
        options.addOption("h", "help", false, "This help directory");
        options.addOption("a", "archive", false, "Build archive");
        options.addOption("c", "compress", false, "Compress archive entries (if -a/--archive)");
        options.addOption("e", "email", true, "User email");
        options.addOption("u", "auth", true, "User auth token");
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

    public static void main(String[] args) throws IOException, CompileExceptionError, URISyntaxException, LibraryException {
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

        Project project = new Project(new DefaultFileSystem(), rootDirectory, buildDirectory);
        if (cmd.hasOption('a')) {
            project.setOption("build_disk_archive", "true");

            if (cmd.hasOption('c')) {
                project.setOption("compress_disk_archive_entries", "true");
            }
        }
        if (cmd.hasOption('e')) {
            project.setOption("email", getOptionsValue(cmd, 'e', null));
        }
        if (cmd.hasOption('u')) {
            project.setOption("auth", getOptionsValue(cmd, 'u', null));
        }

        ClassLoaderScanner scanner = new ClassLoaderScanner();
        project.scan(scanner, "com.dynamo.bob");
        project.scan(scanner, "com.dynamo.bob.pipeline");

        project.setLibUrls(LibraryUtil.getLibraryUrlsFromProject(FilenameUtils.concat(cwd, rootDirectory)));
        for (String command : commands) {
            if (command.equals("resolve")) {
                project.resolveLibUrls(new ConsoleProgress());
                break;
            }
        }
        project.mount(new ClassLoaderResourceScanner());

        Set<String> skipDirs = new HashSet<String>(Arrays.asList(".git", buildDirectory, ".internal"));
        project.findSources(sourceDirectory, skipDirs);
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

    public static String getTexcLibDir() {
        if (texcLibDir == null) {
            try {
                setTexcLibDir();
            } catch (ZipException e) {
                throw new RuntimeException(e);
            } catch (URISyntaxException e) {
                throw new RuntimeException(e);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
        return texcLibDir;
    }

    private enum PlatformType {
        Windows,
        Darwin,
        Linux
    }

    private static PlatformType getPlatform() {
        String os_name = System.getProperty("os.name").toLowerCase();

        if (os_name.indexOf("win") != -1) {
            return PlatformType.Windows;
        } else if (os_name.indexOf("mac") != -1) {
            return PlatformType.Darwin;
        } else if (os_name.indexOf("linux") != -1) {
            return PlatformType.Linux;
        } else {
            throw new RuntimeException(String.format("Could not identify OS: '%s'", os_name));
        }
    }

    private static void setTexcLibDir() throws URISyntaxException, ZipException, IOException {
        URI uri = getJarURI();
        String libSubPath = null;
        PlatformType platform = getPlatform();
        switch (platform) {
        case Windows:
            libSubPath = "lib/win32/texc_shared.dll";
            break;
        case Darwin:
            libSubPath = "lib/x86_64-darwin/libtexc_shared.dylib";
            break;
        case Linux:
            libSubPath = "lib/linux/libtexc_shared.so";
            break;
        }

        File file = FileUtils.toFile(getFile(uri, libSubPath).toURL());
        if (file.exists()) {
            texcLibDir = file.getParentFile().getAbsolutePath();
        } else {
            throw new IOException(String.format("Could not locate '%s'", libSubPath));
        }
    }

    private static URI getJarURI() throws URISyntaxException {
        final ProtectionDomain domain = Bob.class.getProtectionDomain();
        final CodeSource source = domain.getCodeSource();
        final URL url = source.getLocation();
        final URI uri;
        try {
            uri = url.toURI();
        } catch (URISyntaxException e) {
            e.printStackTrace();
            throw e;
        }
        return uri;
    }

    private static URI getFile(final URI where, final String filePath) throws ZipException, IOException {
        final File location;
        final URI fileURI;

        location = new File(where);

        // not in a JAR, just return the path on disk
        if(location.isDirectory()) {
            fileURI = URI.create(where.toString() + filePath);
        } else {
            final ZipFile zipFile = new ZipFile(location);
            try {
                fileURI = extract(zipFile, filePath);
            } finally {
                zipFile.close();
            }
        }

        return (fileURI);
    }

    private static String uniqueTmpDir() {
        final int length = 8;
        String tmp = FileUtils.getTempDirectory().getAbsolutePath();
        String uniquePath = FilenameUtils.concat(tmp, UUID.randomUUID().toString().substring(0, length));
        File f = new File(uniquePath);
        while (f.exists()) {
            uniquePath = FilenameUtils.concat(tmp, UUID.randomUUID().toString().substring(length));
            f = new File(uniquePath);
        }
        f.mkdir();
        f.deleteOnExit();
        return uniquePath;
    }

    private static URI extract(final ZipFile zipFile, final String filePath) throws IOException {
        String fileName = FilenameUtils.getName(filePath);
        String dstPath = FilenameUtils.concat(uniqueTmpDir(), fileName);
        final ZipEntry entry = zipFile.getEntry(filePath);

        if(entry == null) {
            throw new FileNotFoundException("cannot find file: " + filePath + " in archive: " + zipFile.getName());
        }

        final InputStream zipStream  = zipFile.getInputStream(entry);
        OutputStream fileStream = null;

        try {
            final byte[] buf;
            int i;

            fileStream = new FileOutputStream(dstPath);
            buf = new byte[1024];
            i = 0;

            while((i = zipStream.read(buf)) != -1) {
                fileStream.write(buf, 0, i);
            }
        } finally {
            close(zipStream);
            close(fileStream);
        }
        verbose("Extracted '%s' from '%s' to '%s'", filePath, zipFile.getName(), dstPath);

        return (new File(dstPath).toURI());
    }

    private static void close(final Closeable stream) {
        if(stream != null) {
            try {
                stream.close();
            } catch(final IOException ex) {
                ex.printStackTrace();
            }
        }
    }

    public static void verbose(String message, Object... args) {
        if (verbose) {
            System.out.println("Bob: " + String.format(message, args));
        }
    }

}
