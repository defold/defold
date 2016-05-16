package com.dynamo.cr.server.git.archive;

import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.server.ServerException;
import com.google.common.cache.Cache;
import com.google.common.cache.CacheBuilder;
import com.google.common.cache.RemovalListener;
import com.google.common.cache.Weigher;
import com.google.common.util.concurrent.ExecutionError;
import com.google.common.util.concurrent.UncheckedExecutionException;
import com.google.inject.Singleton;
import org.apache.commons.io.IOUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.inject.Inject;
import javax.ws.rs.core.Response.Status;
import java.io.File;
import java.io.IOException;
import java.util.concurrent.ExecutionException;
import java.util.zip.ZipFile;

/**
 * Guava based cache used to store archive file metadata
 */
@Singleton
public class ArchiveCache {
    private static final Logger LOGGER = LoggerFactory.getLogger(ArchiveCache.class);
    private final Cache<String, String> cachedArchives;
    private final Configuration configuration;
    private GitArchiveProvider archiveProvider;

    @Inject
    public ArchiveCache(Configuration configuration) {
        this.configuration = configuration;

        RemovalListener<String, String> removalListener = removal -> {
            try {
                LOGGER.debug("Removing local archive: {}", removal.getValue());

                if (removal.getValue() != null) {
                    File archive = new File(removal.getValue());
                    if (!archive.delete()) {
                        LOGGER.warn("Failed to delete old archive cache entry: {}", removal.getValue());
                    }
                }
            } catch (SecurityException e) {
                LOGGER.warn("Failed to remove old archive file.", e);
            }
        };

        Weigher<String, String> weigher = (key, value) -> {
            long fileSize = new File(value).length();

            // Bytes to kb (files under 0.5 kb will get weight 0)
            return (int) (fileSize / 1024);
        };

        long maximumCacheWeightKb = configuration.getArchiveCacheMaxSize() * 1024; // Mb to kb
        LOGGER.info(String.format("Initializing new archive cache with max size = %d kb", maximumCacheWeightKb));
        cachedArchives = CacheBuilder.newBuilder()
                .maximumWeight(maximumCacheWeightKb)
                .weigher(weigher)
                .removalListener(removalListener)
                .build();

        File baseDirectory = new File(configuration.getArchiveCacheRoot());
        if (!baseDirectory.exists()) {
            if (!baseDirectory.mkdirs()) {
                LOGGER.error("Failed to create missing archive cache root directory: {}",
                        configuration.getArchiveCacheRoot());
                throw new RuntimeException("Failed to initialize archive cache. Missing root directory couldn't be " +
                        "created.");
            }
        }

        for (File dirFile : baseDirectory.listFiles()) {
            try {
                LOGGER.debug("Preloading local archives...");
                loadLocalArchives(dirFile);
            } catch (ExecutionException | UncheckedExecutionException | ExecutionError e) {
                LOGGER.warn("Failed preloading a local archive", e);
            } catch (SecurityException se) {
                LOGGER.warn("Failed reading a local archive", se);
            }
        }
    }

    @Inject
    public void setArchiveProvider(GitArchiveProvider archiveProvider) {
        this.archiveProvider = archiveProvider;
    }

    /**
     * Find all subdirectories (non-recursive) containing a zip-file and attempt loading them to the cache
     *
     * @param archiveDirectory A directory containing archives.
     */
    private void loadLocalArchives(File archiveDirectory)
            throws SecurityException, ExecutionException, UncheckedExecutionException, ExecutionError {

        // Only load non-hidden directories.
        if (!archiveDirectory.isDirectory() || archiveDirectory.getName().startsWith(".")) {
            LOGGER.warn("Invalid archive directory: {}", archiveDirectory.getName());
            return;
        }

        for (File file : archiveDirectory.listFiles()) {
            final String fileKey = archiveDirectory.getName() + file.getName();
            if (!verifyExistingArchive(file, fileKey)) {
                continue;
            }
            LOGGER.debug("Loading a locally cached archive with key: " + fileKey);
            // Init a cache value (synchronized on fileKey)
            getFromCache(fileKey, "", "");
        }
    }

    /**
     * Find an existing zip archive and verify its comment
     *
     * @param directory       Directory containing the archive
     * @param expectedComment Expected zip archive comment
     * @return True if the directory contains archive with expected comment, false otherwise.
     * @throws SecurityException
     */
    private boolean verifyExistingArchive(File directory, String expectedComment) throws SecurityException {
        if (!directory.isDirectory()) {
            return false;
        }

        for (File file : directory.listFiles()) {
            if (file.getName().endsWith(".zip")) {
                return verifyArchiveComment(file, expectedComment);
            }
        }
        return false;
    }

    private boolean verifyArchiveComment(File zipArchive, String expectedComment) {
        ZipFile zipFile = null;
        try {
            zipFile = new ZipFile(zipArchive);
            String comment = zipFile.getComment();
            // Valid archives should have a file key comment
            return expectedComment.equalsIgnoreCase(comment);

        } catch (IOException e) {
            LOGGER.warn("Invalid local zip archive: " + zipArchive.getName(), e);
        } finally {
            IOUtils.closeQuietly(zipFile);
        }
        return false;
    }

    /**
     * Returns the value associated with {@code key} in this cache, first loading that value if
     * necessary. No observable state associated with this cache is modified until loading completes.
     * <p>
     * <p>If another call to {@link #get} is currently loading the value for
     * {@code key}, simply waits for that thread to finish and returns its loaded value. Note that
     * multiple threads can concurrently load values for distinct keys.
     *
     * @throws ServerException if any exception was thrown while loading the value.
     */
    public String get(final String key, final String repositoryName, final String version)
            throws ServerException {

        try {
            String filePathname = getFromCache(key, repositoryName, version);

            // Re-download local archive file if it can't be accessed
            if (new File(filePathname).isFile()) {
                return filePathname;
            } else {
                // This should only happen if the file was manually removed
                invalidateFileCache(filePathname, key);
                return getFromCache(key, repositoryName, version);
            }
        } catch (ExecutionException | UncheckedExecutionException | ExecutionError e) {
            // LoadingCache wraps all exceptions, but we throw server errors from the archive provider etc.
            if (e.getCause() != null) {
                if (ServerException.class.isInstance(e.getCause())) {
                    throw (ServerException) e.getCause();
                }
            }
            throw new ServerException(e.getMessage(), e, Status.INTERNAL_SERVER_ERROR);
        }
    }

    private synchronized void invalidateFileCache(String filePathname, String key)
            throws SecurityException {
        // Check whether the archive already got recreated
        if (new File(filePathname).isFile()) {
            return;
        }
        // Remove entry with no corresponding file from cache
        cachedArchives.invalidate(key);
    }

    private String getFromCache(final String fileKey, final String repositoryName, final String version)
            throws ExecutionException, UncheckedExecutionException, ExecutionError {

        return cachedArchives.get(fileKey, () -> loadFile(fileKey, repositoryName, version));
    }

    /**
     * Discards all entries in the cache.
     */
    public void invalidateAll() {
        cachedArchives.invalidateAll();
    }

    /**
     * Load archive from local repository or using archive provider if local file is missing
     *
     * @param key            Archive key, i.e. sha1
     * @param repositoryName Archive repository name used when the local file is not found
     * @param version        Archive repository version used when the local file is not found
     * @return Archive path.
     * @throws IOException
     * @throws ServerException
     */
    private String loadFile(String key, String repositoryName, String version)
            throws IOException, ServerException {

        File cacheDirectory = getOrCreateArchiveDir(key);

        File localArchive = getExistingArchive(cacheDirectory);
        if (localArchive == null) {
            String targetFilename = cacheDirectory.getCanonicalPath() +
                    File.separator + "archive" + System.currentTimeMillis() + ".zip";
            archiveProvider.createLocalArchive(targetFilename, key, repositoryName, version);

            return targetFilename;
        } else {
            return localArchive.getCanonicalPath();
        }
    }

    private File getOrCreateArchiveDir(String fileKey) {
        String filePath = configuration.getArchiveCacheRoot() + File.separator +
                fileKey.substring(0, 2) + File.separator + fileKey.substring(2);

        File archiveDirectory = new File(filePath);
        if (!archiveDirectory.exists()) {
            archiveDirectory.mkdirs();
        }
        return archiveDirectory;
    }

    private File getExistingArchive(File cacheFolder) {
        for (File file : cacheFolder.listFiles()) {
            if (file.isFile() && file.getName().endsWith(".zip")) {
                return file;
            }
        }
        return null;
    }
}
