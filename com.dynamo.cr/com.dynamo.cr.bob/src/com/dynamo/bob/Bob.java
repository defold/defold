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

public class Bob {

    private static String texcLibDir = null;

    private static CommandLine parse(String[] args) {
        Options options = new Options();
        options.addOption("r", "root", true, "Build root directory. Default is current directory");
        options.addOption("o", "out", true, "Output directory. Default is \"build/default\"");
        options.addOption("i", "input", true, "Source directory. Default is current directory");
        options.addOption("h", "help", false, "This help directory");
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

    public static void main(String[] args) throws IOException, CompileExceptionError, URISyntaxException {
        System.setProperty("java.awt.headless", "true");
        String cwd = new File(".").getAbsolutePath();
        String libDir = getTexcLibDir();
        String prop = "jna.library.path";
        System.setProperty(prop, System.getProperty(prop) + File.pathSeparator + libDir);

        CommandLine cmd = parse(args);
        String buildDirectory = getOptionsValue(cmd, 'o', "build/default");
        String rootDirectory = getOptionsValue(cmd, 'r', cwd);
        String sourceDirectory = getOptionsValue(cmd, 'i', ".");

        String[] commands = cmd.getArgs();
        if (commands.length == 0) {
            commands = new String[] { "build" };
        }

        Project project = new Project(new DefaultFileSystem(), rootDirectory, buildDirectory);

        ClassLoaderScanner scanner = new ClassLoaderScanner();
        project.scan(scanner, "com.dynamo.bob");
        project.scan(scanner, "com.dynamo.bob.pipeline");

        Set<String> skipDirs = new HashSet<String>(Arrays.asList(".git", buildDirectory));

        project.findSources(sourceDirectory, skipDirs);
        List<TaskResult> result = project.build(new ConsoleProgress(), commands);
        boolean ret = true;
        for (TaskResult taskResult : result) {
            if (!taskResult.isOk()) {
                ret = false;
            }
        }
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
        // Fallback for testing
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

    private static String getPlatform() {
        String os_name = System.getProperty("os.name").toLowerCase();

        if (os_name.indexOf("win") != -1)
            return "win32";
        else if (os_name.indexOf("mac") != -1)
            return "darwin";
        else if (os_name.indexOf("linux") != -1)
            return "linux";
        return null;
    }

    private static void setTexcLibDir() throws URISyntaxException, ZipException, IOException {
        URI uri = getJarURI();
        String prefix = "";
        String ext = "";
        String platform = getPlatform();
        if (platform.equals("win32")) {
            ext = ".dll";
        } else if (platform.equals("darwin")) {
            prefix = "lib";
            ext = ".dylib";
        } else if (platform.equals("linux")) {
            prefix = "lib";
            ext = ".so";
        }
        String path = prefix + "texc_shared" + ext;
        File file = FileUtils.toFile(getFile(uri, path).toURL());
        if (!file.exists()) {
            uri = new File(uri.getPath() + "build/").toURI();
            file = FileUtils.toFile(getFile(uri, path).toURL());
        }
        texcLibDir = FileUtils.toFile(getFile(uri, path).toURL()).getParentFile().getAbsolutePath();
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

    private static URI extract(final ZipFile zipFile, final String filePath) throws IOException {
        String fileName = FilenameUtils.getName(filePath);
        String dstPath = FilenameUtils.concat(FileUtils.getTempDirectory().getAbsolutePath(), fileName);
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
}
