package com.dynamo.cr.server.git;

import com.google.inject.Singleton;
import org.apache.commons.io.FileUtils;
import org.eclipse.jgit.api.Git;
import org.eclipse.jgit.api.errors.GitAPIException;
import org.eclipse.jgit.api.errors.JGitInternalException;
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

    public Properties garbageCollect(String repositoryRoot, String expirationDate) throws IOException {
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

                if(causeMessage != null && causeMessage.contains("packed-refs")) {
                    deleteLockFile(repositoryRoot);
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

    private void deleteLockFile(String repositoryRoot) throws IOException {
        // Assuming this is a bare repo and no other gc process is running
        File refLock = new File(repositoryRoot + File.separator + "packed-refs.lock");
        if(refLock.exists()) {
            LOGGER.error("Removing stale packed-refs lock: " + refLock.getCanonicalPath());
            FileUtils.deleteQuietly(refLock);
        }
    }
}
