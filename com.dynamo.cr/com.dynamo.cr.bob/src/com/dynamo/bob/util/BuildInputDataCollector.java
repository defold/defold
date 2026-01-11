package com.dynamo.bob.util;

import com.dynamo.bob.archive.EngineVersion;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;

import javax.xml.bind.DatatypeConverter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.FileWriter;
import java.nio.file.Files;
import java.security.MessageDigest;

public class BuildInputDataCollector {
    private static final String DATA_FILE_NAME = "build_input_data.json";

    private static String[] bobArguments;
    private static Map<String, File> dependencies;

    public static void setArgs(String[] args) {
        bobArguments = args;
    }

    public static void setDependencies(Map<String, File> libFiles) {
        dependencies = libFiles;
    }

    public static void saveDataAsJson(String rootDirectory, File bundleOutputDirectory, String defoldSdk) {
        TimeProfiler.start("BuildInputDataCollector");
        Map<String, Object> data = new HashMap<>();
        data.put("bob_arguments", bobArguments);

        Map<String, String> bobVersion = new HashMap<>();
        bobVersion.put("sha1", EngineVersion.sha1);
        bobVersion.put("version", EngineVersion.version);
        bobVersion.put("timestamp", EngineVersion.timestamp);
        data.put("bob_arguments", bobVersion);
        data.put("defold_sdk", defoldSdk);

        Map<String, String> gitInfo = getGitInfoIfAvailable(rootDirectory);
        if (gitInfo != null) {
            data.put("project_git", gitInfo);
        }

        if (dependencies != null && !dependencies.isEmpty()) {
            List<Map<String, String>> depsList = new ArrayList<>();
            for (Map.Entry<String, File> entry : dependencies.entrySet()) {
                File file = entry.getValue();
                if (file.exists() && file.isFile()) {
                    try {
                        byte[] fileBytes = Files.readAllBytes(file.toPath());
                        MessageDigest md = MessageDigest.getInstance("MD5");
                        md.update(fileBytes);
                        Map<String, String> depEntry = new HashMap<>();
                        depEntry.put("link", entry.getKey());
                        depEntry.put("md5", DatatypeConverter.printHexBinary(md.digest()).toUpperCase());
                        depsList.add(depEntry);
                    } catch (Exception e) {
                        throw new RuntimeException("Failed to generate dependencies info", e);
                    }
                }
            }
            if (!depsList.isEmpty()) {
                data.put("dependencies", depsList);
            }
        }

        File outputFile = new File(bundleOutputDirectory, DATA_FILE_NAME);
        try (FileWriter writer = new FileWriter(outputFile)) {
            ObjectMapper mapper = new ObjectMapper().enable(SerializationFeature.INDENT_OUTPUT);
            mapper.writeValue(writer, data);
        } catch (IOException e) {
            throw new RuntimeException("Failed to write data to JSON file", e);
        }
        TimeProfiler.stop();
    }

    private static Map<String, String> getGitInfoIfAvailable(String rootDirectory) {
        File gitDir = new File(rootDirectory, ".git");
        if (!gitDir.exists()) {
            return null;
        }

        try {
            Map<String, String> gitInfo = new HashMap<>();

            String sha = runGitCommand(rootDirectory, "rev-parse", "HEAD");
            if (sha != null) {
                gitInfo.put("sha1", sha);
            }

            String branch = runGitCommand(rootDirectory, "rev-parse", "--abbrev-ref", "HEAD");
            if (branch != null) {
                gitInfo.put("branch", branch);
            }

            String origin = runGitCommand(rootDirectory, "config", "--get", "remote.origin.url");
            if (origin != null) {
                gitInfo.put("origin", origin);
            }

            return gitInfo;
        } catch (Exception e) {
            return null;
        }
    }

    private static String runGitCommand(String directory, String... command) throws IOException, InterruptedException {
        List<String> cmd = new ArrayList<>();
        cmd.add("git");
        cmd.addAll(Arrays.asList(command));
        ProcessBuilder pb = new ProcessBuilder(cmd);
        pb.directory(new File(directory));
        pb.redirectErrorStream(true);
        Process process = pb.start();

        try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()))) {
            String result = reader.readLine();
            int exitCode = process.waitFor();
            if (exitCode == 0 && result != null && !result.isEmpty()) {
                return result.trim();
            }
        }
        return null;
    }
}
