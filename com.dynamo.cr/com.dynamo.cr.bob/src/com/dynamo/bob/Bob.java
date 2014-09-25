package com.dynamo.bob;

import java.io.File;
import java.io.IOException;
import java.net.URISyntaxException;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.apache.commons.cli.PosixParser;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.util.LibraryUtil;

public class Bob {

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

    public static void verbose(String message, Object... args) {
        if (verbose) {
            System.out.println("Bob: " + String.format(message, args));
        }
    }

}
