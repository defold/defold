package com.defold.editor;

import java.io.File;
import java.io.FileFilter;
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
import org.apache.commons.io.filefilter.WildcardFileFilter;
import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.ArrayNode;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class Updater {

    private static boolean shouldUpdateLauncher() {
        String env = System.getenv("DEFOLD_UPDATE_LAUNCHER");
        return env != null && (env.equalsIgnoreCase("1") || env.equalsIgnoreCase("true") || env.equalsIgnoreCase("yes"));
    }

    private static File launcherFile(File resourcesPath) {
        String path = System.getProperty("defold.launcherpath");
        if (path == null) {
            // On old versions we have to infer launcher path from resources-path
            Platform platform = Platform.getHostPlatform();
            if (platform.os.equals("darwin")) {
                path = new File(resourcesPath.getParentFile(), "MacOS/Defold").getAbsolutePath();
            } else {
                path = new File(resourcesPath, "Defold" + platform.getExeSuffix()).getAbsolutePath();
            }
        }
        logger.info("launcher path: {}", path);
        return new File(path);
    }

    public static final class PendingUpdate {
        private Map<String, File> files;
        private File config;
        private File launcher;
        public String version;
        public String sha1;

        PendingUpdate(Map<String, File> files, File config, File launcher, String version, String sha1) {
            this.files = files;
            this.config = config;
            this.launcher = launcher;
            this.version = version;
            this.sha1 = sha1;
        }

        private File packageFile(File resourcesPath, String toName) {
            return new File(new File(resourcesPath, "packages"), toName);
        }

        private void copyFile(File from, File resourcesPath, String toName) throws IOException {
            File to = packageFile(resourcesPath, toName);
            logger.info("copying {} -> {}", new Object[] {from, to});
            FileUtils.copyFile(from, to);
        }

        private void apply(File resourcesPath, Map<String, File> files) throws IOException {
            for (Entry<String, File> entry : files.entrySet()) {
                copyFile(entry.getValue(), resourcesPath, entry.getKey());
            }
        }

	public void deleteFiles() {
	    for (Entry<String, File> entry : files.entrySet()) {
		FileUtils.deleteQuietly(entry.getValue());
	    }
	    FileUtils.deleteQuietly(config);
	}

        /**
         * Installs the update
         * @throws IOException
         */
        public void install(File resourcesPath) throws IOException {
            apply(resourcesPath, files);
            File toConfig = new File(resourcesPath, "config");
            logger.info("copying {} -> {}", new Object[] {config, toConfig});
            FileUtils.copyFile(config, toConfig);

            if (shouldUpdateLauncher()) {
                // Install new launcher. Note that the new launcher will not be used until next relaunch of the application
                logger.info("updating launcher");
                File currentLauncher = launcherFile(resourcesPath);
                File newLauncher = launcher;

                // Try to remove old launchers as we can't remove the file immediately on Windows.
                // This might also fail on Windows if two or more updates are performed as the initial launcher file, renamed to OLD-LAUNCHER-XXXX, is locked
                // However, on next relaunch the file will be removed
                File[] oldLaunchers = currentLauncher.getParentFile().listFiles((FileFilter) new WildcardFileFilter("OLD-LAUNCHER*"));
                for (File f : oldLaunchers) {
                    try {
                        f.delete();
                    } catch (Throwable e) {}
                }
                File oldLauncher = File.createTempFile("OLD-LAUNCHER", "", currentLauncher.getParentFile());
                logger.info("renaming {} -> {}", currentLauncher, oldLauncher);
                currentLauncher.renameTo(oldLauncher);
                logger.info("copying {} -> {}", newLauncher, currentLauncher);
                FileUtils.copyFile(newLauncher, currentLauncher);
                currentLauncher.setExecutable(true);
            }
        }
    }

    private static Logger logger = LoggerFactory.getLogger(Updater.class);

    private String updateUrl;
    private Path tempDirectory;
    private ObjectMapper mapper;

    public Updater(String updateUrl) throws IOException {
        this.updateUrl = updateUrl;
        this.tempDirectory = Files.createTempDirectory(null);
        this.mapper = new ObjectMapper();

        // Delete temp files at shutdown.
        File tempDirectory = this.tempDirectory.toFile();
        Runtime.getRuntime().addShutdownHook(new Thread(() -> FileUtils.deleteQuietly(tempDirectory)));

        if (shouldUpdateLauncher()) {
            logger.info("Launcher updates enabled");
        }
    }

    private File download(String packagesUrl, String url) throws IOException {
        URI uri = UriBuilder.fromUri(packagesUrl).path(url).build();
        Path f = Files.createTempFile(tempDirectory, null, null);
        logger.info("downloading {} -> {}", new Object[] {url, f.toString()});
        FileUtils.copyURLToFile(uri.toURL(), f.toFile());
        return f.toFile();
    }

    private JsonNode fetchJson(URI uri) throws IOException {
        return mapper.readValue(IOUtils.toString(uri), JsonNode.class);
    }

    private URI makeURI(String baseUri, String path) {
        return UriBuilder.fromUri(baseUri).path(path).build();
    }

    /**
     * Check for updates
     * @return {@link PendingUpdate} when an update is pending. Otherwise null.
     * @throws IOException
     */
    public PendingUpdate check(String currentSha1) throws IOException {
        JsonNode update = fetchJson(makeURI(updateUrl, "update.json"));
        String packagesUrl = update.get("url").asText();
        URI packagesUri = makeURI(packagesUrl, "manifest.json");
        if (!"https".equals(packagesUri.getScheme())) {
            logger.warn(String.format("update URL not secure '%s'", packagesUrl));
        }
        String packagesHost = packagesUri.getHost();
        if (packagesHost == null || (!packagesHost.endsWith(".defold.com")) && !packagesHost.equals("localhost")) {
            logger.error(String.format("forbidden host in update URL '%s'", packagesUrl));
            return null;
        }
        JsonNode manifest = fetchJson(packagesUri);
        String sha1 = manifest.get("sha1").asText();
        String version = manifest.get("version").asText();

        Platform platform = Platform.getHostPlatform();

        Map<String, File> files = new HashMap<>();
        if (!sha1.equals(currentSha1)) {
            logger.info("new version found {}", sha1);

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
            File launcher = null;
            if (shouldUpdateLauncher()) {
                launcher = download(packagesUrl, String.format("launcher-%s%s", platform.getPair(), platform.getExeSuffix()));
            }
            return new PendingUpdate(files, config, launcher, version, sha1);
        }
        return null;
    }
}
