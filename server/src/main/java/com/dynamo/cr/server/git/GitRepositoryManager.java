package com.dynamo.cr.server.git;

import com.google.common.io.Files;
import com.google.inject.Singleton;
import org.apache.commons.io.FileUtils;
import org.eclipse.jgit.api.Git;
import org.eclipse.jgit.api.errors.GitAPIException;
import org.eclipse.jgit.api.errors.JGitInternalException;
import org.eclipse.jgit.lib.StoredConfig;
import org.eclipse.jgit.util.GitDateParser;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.IOException;
import java.text.ParseException;
import java.util.Calendar;
import java.util.Properties;

@Singleton
public class GitRepositoryManager {
    private static final Logger LOGGER = LoggerFactory.getLogger(GitRepositoryManager.class);

    /**
     * Performs garbage collection on a repository.
     *
     * @param repositoryRoot Repository root path.
     * @param expirationDate Expiration date (as string parsable by @{@link GitDateParser}).
     * @return Statistics from garbage collection, or empty object of garbage collection not performed.
     * @throws IOException
     */
    Properties garbageCollect(String repositoryRoot, String expirationDate) throws IOException {
        Git git = null;

        try {
            git = Git.open(new File(repositoryRoot));

            return git.gc()
                    .setAggressive(false)
                    .setExpire(GitDateParser.parse(expirationDate, Calendar.getInstance()))
                    .call();
        } catch (GitAPIException e) {
            LOGGER.error(String.format("Could not run garbage collect on %s", repositoryRoot), e);
        } catch(JGitInternalException e) {
            String causeMessage = "";
            Throwable cause = e.getCause();

            if(cause != null) {
                causeMessage = cause.getMessage();

                // Try to delete lock-file.
                if(causeMessage != null && causeMessage.contains("packed-refs")) {
                    // Assuming this is a bare repo and no other gc process is running
                    File refLock = new File(repositoryRoot + File.separator + "packed-refs.lock");
                    if(refLock.exists()) {
                        LOGGER.error("Removing stale packed-refs lock: " + refLock.getCanonicalPath());
                        FileUtils.deleteQuietly(refLock);
                    }
                }
            }

            LOGGER.error(String.format("Could not run garbage collect on %s: %s", repositoryRoot, causeMessage), e);
        } catch (ParseException e) {
            LOGGER.error(String.format("Could not parse git gc expiration date: %s", expirationDate), e);
        } finally {
            if (git != null) {
                git.close();
            }
        }

        return new Properties(); // Empty properties list
    }

    /**
     * Create a new repository by cloning from specified origin.
     *
     * NOTE: All git history from origin will be reset.
     *
     * @param targetPath Target path of new repository.
     * @param originPath Origin path.
     * @throws GitAPIException
     * @throws IllegalStateException
     * @throws IOException
     */
    public void createRepository(File targetPath, String originPath)
            throws GitAPIException, IllegalStateException, IOException {

        File tempDir = Files.createTempDir();

        try {
            // Clone from origin to temporary directory.
            Git.cloneRepository()
                    .setURI(originPath)
                    .setDirectory(tempDir.getAbsoluteFile())
                    .call();

            // Re-init the git repository, all history will be removed
            deleteGitData(tempDir);
            Git tmpGit = Git.init().setDirectory(tempDir.getAbsoluteFile()).call();

            // Add all files to new baseline
            tmpGit.add().addFilepattern(".").call();
            tmpGit.commit().setMessage("Initial commit").call();

            // Bare clone to final destination
            Git git = Git.cloneRepository()
                    .setBare(true)
                    .setURI(tempDir.getAbsolutePath())
                    .setDirectory(targetPath.getAbsoluteFile())
                    .call();
            updateGitConfig(git, originPath);
        } finally {
            FileUtils.deleteQuietly(tempDir);
        }
    }

    /**
     * Create a new empty repository.
     *
     * @param targetPath Target path of new repository.
     * @throws GitAPIException
     * @throws IOException
     */
    public void createRepository(File targetPath) throws GitAPIException, IOException {
        Git git = Git.init().setBare(true).setDirectory(targetPath.getAbsoluteFile()).call();
        updateGitConfig(git, null);
    }

    private void deleteGitData(File directory) throws IOException {
        FileUtils.deleteDirectory(new File(directory, ".git"));
    }

    private void updateGitConfig(Git git, String origin) throws IOException {
        StoredConfig config = git.getRepository().getConfig();
        if (origin != null) {
            config.setString("remote", "origin", "url", origin);
        }
        config.setBoolean("http", null, "receivepack", true);
        config.setString("core", null, "sharedRepository", "group");
        config.save();
    }
}
