package com.defold.editor;

import java.io.File;
import java.io.IOException;
import java.net.URI;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;

import javax.ws.rs.core.UriBuilder;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.ArrayNode;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class Updater {
    private static Logger logger = LoggerFactory.getLogger(Updater.class);

    private String updateUrl;
    private Path tempDirectory;
    private File resourcesPath;
    private String currentSha1;

    private ObjectMapper mapper;

    public static class UpdateInfo {
        public String version;
        public String sha1;

        public UpdateInfo(String version, String sha1) {
            this.version = version;
            this.sha1 = sha1;
        }
    }

    public Updater(String updateUrl, String resourcesPath, String currentSha1) throws IOException {
        this.updateUrl = updateUrl;
        this.tempDirectory = Files.createTempDirectory(null);
        this.resourcesPath = new File(resourcesPath);
        this.currentSha1 = currentSha1;
        this.mapper = new ObjectMapper();
    }

    private File download(String packagesUrl, String url) throws IOException {
        URI uri = UriBuilder.fromUri(packagesUrl).path(url).build();
        Path f = Files.createTempFile(tempDirectory, null, null);
        logger.info("downloading {} -> {}", new Object[] {url, f.toString()});
        FileUtils.copyURLToFile(uri.toURL(), f.toFile());
        return f.toFile();
    }

    private File packageFile(String toName) {
        return new File(new File(resourcesPath, "packages"), toName);
    }

    private void copyFile(File from, String toName) throws IOException {
        File to = packageFile(toName);
        logger.info("copying {} -> {}", new Object[] {from, to});
        FileUtils.copyFile(from, to);
    }

    private boolean apply(Map<String, File> files) throws IOException {
        for (Entry<String, File> entry : files.entrySet()) {
            File to = packageFile(entry.getKey());
            if (to.exists()) {
                logger.error("update failed. file {} already exists", to);
                return false;
            }
        }

        for (Entry<String, File> entry : files.entrySet()) {
            copyFile(entry.getValue(), entry.getKey());
        }
        return true;
    }

    private JsonNode fetchJson(URI uri) throws IOException {
        return mapper.readValue(IOUtils.toString(uri), JsonNode.class);
    }

    private URI makeURI(String baseUri, String path) {
        return UriBuilder.fromUri(baseUri).path(path).build();
    }

    /**
     * Check for updates
     * @return {@link UpdateInfo} when an update is applied successfully. Otherwise null.
     * @throws IOException
     */
    public UpdateInfo check() throws IOException {
        JsonNode update = fetchJson(makeURI(updateUrl, "update.json"));
        String packagesUrl = update.get("url").asText();


        JsonNode manifest = fetchJson(makeURI(packagesUrl, "manifest.json"));
        String sha1 = manifest.get("sha1").asText();
        String version = manifest.get("version").asText();

        Map<String, File> files = new HashMap<>();
        if (!sha1.equals(currentSha1)) {
            logger.info("new version found {}", sha1);
            currentSha1 = sha1;

            ArrayNode packages = (ArrayNode) manifest.get("packages");
            for (JsonNode pkg : packages) {
                if (pkg.get("action").asText().equals("copy")) {
                    String url = pkg.get("url").asText();
                    File f = download(packagesUrl, url);
                    files.put(FilenameUtils.getName(url), f);
                } else {
                    logger.warn("package not supported {}", pkg);
                    return null;
                }
            }
            File config = download(packagesUrl, "config");

            if (apply(files)) {
                File toConfig = new File(resourcesPath, "config");
                logger.info("copying {} -> {}", new Object[] {config, toConfig});
                FileUtils.copyFile(config, toConfig);
                return new UpdateInfo(version, sha1);
            }
        }
        return null;
    }
}

