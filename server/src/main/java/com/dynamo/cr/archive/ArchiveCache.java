package com.dynamo.cr.archive;

import java.io.File;
import java.io.IOException;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.zip.ZipFile;

import javax.inject.Inject;
import javax.ws.rs.core.Response.Status;

import org.apache.commons.io.IOUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.server.ServerException;
import com.google.common.cache.Cache;
import com.google.common.cache.CacheBuilder;
import com.google.common.cache.RemovalListener;
import com.google.common.cache.RemovalNotification;
import com.google.common.cache.Weigher;
import com.google.common.util.concurrent.ExecutionError;
import com.google.common.util.concurrent.UncheckedExecutionException;
import com.google.inject.Singleton;

/**
 * Guava based cache used to store archive file metadata
 */
@Singleton
public class ArchiveCache {
    private Cache<String, String> cachedArchives;
    protected static Logger logger = LoggerFactory.getLogger(ArchiveCache.class);
    private Configuration configuration;
    private GitArchiveProvider archiveProvider;

    @Inject
    public void setArchiveProvider(GitArchiveProvider archiveProvider) {
        this.archiveProvider = archiveProvider;
    }

    @Inject
    public ArchiveCache(Configuration configuration) {
        this.configuration = configuration;

        RemovalListener<String, String> removalListener = new RemovalListener<String, String>() {
            public void onRemoval(RemovalNotification<String, String> removal) {
                try {
                    logger.debug("Removing local archive: " + removal.getValue());

                    File archive = new File(removal.getValue());
                    archive.delete();
                } catch (SecurityException se) {
                    logger.warn("Failed to remove old archive file", se);
                }
            }
        };

        Weigher<String, String> weigher = new Weigher<String, String>() {
            public int weigh(String key, String value) {
                long fileSize = new File(value).length();

                // Bytes to kb (files under 0.5 kb will get weight 0)
                return (int) (fileSize / 1024);
            }
          };

        cachedArchives = CacheBuilder.newBuilder()
                .maximumWeight(configuration.getArchiveCacheMaxSize())
                .weigher(weigher)
                .removalListener(removalListener)
                .build();

        String archiveRoot = configuration.getArchiveCacheRoot();
        File baseDirectory = new File(archiveRoot);
        if (!baseDirectory.exists()) {
            baseDirectory.mkdirs();
        }

       for (File dirFile : baseDirectory.listFiles()) {
           try {

               loadLocalArchives(dirFile);
           } catch(ExecutionException | UncheckedExecutionException | ExecutionError e) {
               logger.warn("Failed preloading a local archive", e);
           } catch (SecurityException se) {
               logger.warn("Failed reading a local archive", se);
           }
       }
    }

    /**
     * Find all subdirectories (non-recursive) containing a zipfile and attempt loading them to the cache
     * @param baseDirectory
     */
    private void loadLocalArchives(File dirFile)
            throws SecurityException, ExecutionException, UncheckedExecutionException, ExecutionError  {

        if (!dirFile.isDirectory() || dirFile.getName().startsWith(".")) {
            return;
        }

        for(File file : dirFile.listFiles()) {
            if(!verifyExistingArchive(file)) {
                continue;
            }

            final String fileKey = dirFile.getName() + file.getName();
            logger.debug("Loading a locally cached archive with key: " + fileKey);

            // Init a cache value (synchronized on fileKey)
            getFromCache(fileKey, "", "");
        }
    }

    /**
     * Find an existing zip archive and verify its comment
     * @param directory Directory containing the archive
     * @return
     * @throws SecurityException
     */
    private boolean verifyExistingArchive(File directory) throws SecurityException {
        if(!directory.isDirectory()) {
            return false;
        }

        for(File file : directory.listFiles()) {
            if(file.getName().endsWith(".zip")) {
                String expectedComment =
                        directory.getParentFile().getName() + directory.getName();

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
            return comment.equals(expectedComment);

        } catch (IOException e) {
            logger.warn("Invalid local zip file", e);
        } finally {
            IOUtils.closeQuietly(zipFile);
        }
        return false;
    }

    /**
     * Returns the value associated with {@code key} in this cache, first loading that value if
     * necessary. No observable state associated with this cache is modified until loading completes.
     *
     * <p>If another call to {@link #get} is currently loading the value for
     * {@code key}, simply waits for that thread to finish and returns its loaded value. Note that
     * multiple threads can concurrently load values for distinct keys.
     * @throws ServerException if any exception was thrown while loading the value.
     */
    public String get(final String key, final String repositoryName, final String version)
            throws ServerException {

        try {
            String filePathname = getFromCache(key, repositoryName, version);

            // Re-download local archive file if it can't be accessed
            if(!isExistingFile(filePathname)) {
                // This should not normally happen - only if the file is manually removed
                return refreshFileCache(filePathname, key, repositoryName, version);
            }
            return filePathname;

        } catch (ExecutionException | UncheckedExecutionException | ExecutionError e) {
            // LoadingCache wraps all exceptions, but we throw server errors from the archive provider etc.
            if (e != null && e.getCause() != null) {
                if(ServerException.class.isInstance(e.getCause())) {
                    throw (ServerException)e.getCause();
                }
            }
            throw new ServerException(e.getMessage(), e, Status.INTERNAL_SERVER_ERROR);
        }
    }

    private synchronized String refreshFileCache(String filePathname, String key, String repositoryName, String version)
            throws ServerException, ExecutionException, UncheckedExecutionException {

        if(isExistingFile(filePathname)) {
            return filePathname;
        }
        cachedArchives.invalidate(key);
        filePathname = getFromCache(key, repositoryName, version);

        if(!isExistingFile(filePathname)) {
            throw new ServerException("Failed to open local archive file: " + filePathname,
                    Status.INTERNAL_SERVER_ERROR);
        } else {
            return filePathname;
        }
    }

    private String getFromCache(final String fileKey, final String repositoryName, final String version)
            throws ExecutionException, UncheckedExecutionException, ExecutionError {

        String filePathname = cachedArchives.get(fileKey, new Callable<String>() {
            @Override
            public String call() throws ServerException, IOException  {
                return loadFile(fileKey, repositoryName, version);
            }
        });
        return filePathname;
    }

    private boolean isExistingFile(String filePathname) {
        if(filePathname == null) {
            return false;
        }

        File zipFile = new File(filePathname);
        if(!zipFile.exists() || zipFile.isDirectory()) {
            return false;
        }
        return true;
    }

    /**
     * Discards all entries in the cache.
     */
    public void invalidateAll() {
        cachedArchives.invalidateAll();
    }

    /**
     * Load archive from local repository or using archive provider if local file is missing
     * @param key Archive key, i.e. sha1
     * @param repositoryName Archive repository name used when the local file is not found
     * @param version Archive repository version used when the local file is not found
     * @return
     * @throws IOException
     * @throws ServerException
     */
    public String loadFile(String key, String repositoryName, String version)
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
        if(!archiveDirectory.exists()) {
            archiveDirectory.mkdirs();
        }
        return archiveDirectory;
    }

    private File getExistingArchive(File cacheFolder) throws IOException {
        for (File file : cacheFolder.listFiles()) {
            if (file.isFile() && file.getName().endsWith(".zip")) {
                return file;
            }
        }
        return null;
    }
}
