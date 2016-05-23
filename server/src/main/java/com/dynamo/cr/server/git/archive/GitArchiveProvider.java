package com.dynamo.cr.server.git.archive;

import com.dynamo.cr.server.ServerException;
import com.google.common.io.Files;
import com.google.inject.Singleton;
import org.apache.commons.io.FileUtils;
import org.eclipse.jgit.api.CloneCommand;
import org.eclipse.jgit.api.Git;
import org.eclipse.jgit.api.errors.*;
import org.eclipse.jgit.lib.ObjectId;
import org.eclipse.jgit.lib.Ref;
import org.eclipse.jgit.lib.Repository;
import org.eclipse.jgit.revwalk.RevCommit;
import org.eclipse.jgit.revwalk.RevObject;
import org.eclipse.jgit.revwalk.RevTag;
import org.eclipse.jgit.revwalk.RevWalk;
import org.eclipse.jgit.storage.file.FileRepositoryBuilder;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.inject.Inject;
import javax.ws.rs.core.Response.Status;
import java.io.File;
import java.io.IOException;
import java.nio.file.Paths;

@Singleton
public class GitArchiveProvider {
    private static final Logger LOGGER = LoggerFactory.getLogger(GitArchiveProvider.class);

    @Inject
    public GitArchiveProvider() {
    }

    public void createLocalArchive(String archivePathname, String fileKey, String repositoryName, String version)
            throws IOException, ServerException {

        File cloneToDirectory = null;

        try {
            cloneToDirectory = Files.createTempDir();

            // NOTE: No shallow clone support in current JGit
            CloneCommand clone = Git.cloneRepository().setURI(repositoryName).setBare(false).setDirectory(cloneToDirectory);

            try {
                Git git = clone.call();
                git.checkout().setName(fileKey).call();
            } catch (JGitInternalException | RefAlreadyExistsException e) {
                throw new ServerException("Failed to checkout repo", e, Status.INTERNAL_SERVER_ERROR);
            } catch (RefNotFoundException | InvalidRefNameException e) {
                throw new ServerException(String.format("Version not found: %s", version), e,
                        Status.INTERNAL_SERVER_ERROR);
            } catch (GitAPIException e) {
                throw new ServerException("Failed to checkout repo", e, Status.INTERNAL_SERVER_ERROR);
            }

            ZipUtil.zipFilesAtomic(cloneToDirectory, Paths.get(archivePathname).toFile(), fileKey);

        } catch (IOException e) {
            throw new IOException("Failed to clone and zip project", e);
        } finally {
            // Always delete the clone-to directory
            try {
                if (cloneToDirectory != null) {
                    FileUtils.deleteDirectory(cloneToDirectory);
                }
            } catch (IOException | IllegalArgumentException e) {
                LOGGER.error("Failed to remove temporary clone directory", e);
            }
        }
    }

    public static String getSHA1ForName(String repository, String name) throws IOException, IllegalArgumentException {

        Repository repo = null;
        RevWalk walk = null;

        try {
            FileRepositoryBuilder builder = new FileRepositoryBuilder();
            repo = builder.setGitDir(new File(repository)).findGitDir().build();

            walk = new RevWalk(repo);
            Ref p = repo.getRef(name);
            if (p == null) {
                // if not ref
                ObjectId objId = repo.resolve(name);
                if (objId != null) {
                    return objId.getName();
                }
                return null;
            }
            RevObject object = walk.parseAny(p.getObjectId());
            if (object instanceof RevTag) {
                RevTag tag = (RevTag) object;
                return tag.getObject().getName();
            } else if (object instanceof RevCommit) {
                RevCommit commit = (RevCommit) object;
                return commit.getName();
            } else {
                throw new IllegalArgumentException(String.format("Unknown object: %s", object.toString()));
            }
        } finally {
            if (walk != null) {
                walk.dispose();
            }
            if (repo != null) {
                repo.close();
            }
        }
    }
}
