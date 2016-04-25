package com.dynamo.cr.archive;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.Collection;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import javax.inject.Inject;
import javax.ws.rs.core.Response.Status;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.io.filefilter.AbstractFileFilter;
import org.apache.commons.io.filefilter.IOFileFilter;
import org.apache.commons.io.filefilter.TrueFileFilter;
import org.eclipse.jgit.api.CloneCommand;
import org.eclipse.jgit.api.Git;
import org.eclipse.jgit.api.errors.GitAPIException;
import org.eclipse.jgit.api.errors.InvalidRefNameException;
import org.eclipse.jgit.api.errors.JGitInternalException;
import org.eclipse.jgit.api.errors.RefAlreadyExistsException;
import org.eclipse.jgit.api.errors.RefNotFoundException;
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

import com.dynamo.cr.server.ServerException;
import com.google.common.io.Files;
import com.google.inject.Singleton;

@Singleton
public class GitArchiveProvider {
    protected static Logger logger = LoggerFactory.getLogger(GitArchiveProvider.class);

    @Inject
    public GitArchiveProvider() {
    }

    public void createLocalArchive(String archivePathname, String fileKey, String repositoryName, String version)
            throws IOException, ServerException {

        File cloneTo = null;
        File localFile = null;

        try {
            cloneTo = Files.createTempDir();

            // NOTE: No shallow clone support in current JGit
            CloneCommand clone = Git.cloneRepository().setURI(repositoryName).setBare(false).setDirectory(cloneTo);

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

            localFile = File.createTempFile("archive", ".zip");
            zipFiles(cloneTo, localFile, fileKey);

            // nio supports atomic file move
            java.nio.file.Files.move(localFile.toPath(), Paths.get(archivePathname), StandardCopyOption.ATOMIC_MOVE);

        } catch (IOException e) {
            // NOTE: Only delete zip-file on error as we need the file when
            // streaming.
            localFile.delete();
            throw new IOException("Failed to clone and zip project", e);
        } finally {
            // NOTE: We always delete temporary cloneTo directory
            try {
                FileUtils.deleteDirectory(cloneTo);
            } catch (IOException | IllegalArgumentException e) {
                logger.error("Failed to remove temporary clone directory", e);
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

    private static void zipFiles(File directory, File zipFile, String comment) throws IOException {
        ZipOutputStream zipOut = null;

        try {
            zipOut = new ZipOutputStream(new FileOutputStream(zipFile));
            zipOut.setComment(comment);
            IOFileFilter dirFilter = new AbstractFileFilter() {
                @Override
                public boolean accept(File file) {
                    return !file.getName().equals(".git");
                }
            };

            Collection<File> files = FileUtils.listFiles(directory, TrueFileFilter.INSTANCE, dirFilter);
            int prefixLength = directory.getAbsolutePath().length();
            for (File file : files) {
                String p = file.getAbsolutePath().substring(prefixLength);
                if (p.startsWith("/")) {
                    p = p.substring(1);
                }
                ZipEntry ze = new ZipEntry(p);
                zipOut.putNextEntry(ze);
                FileUtils.copyFile(file, zipOut);
            }
        } finally {
            IOUtils.closeQuietly(zipOut);
        }
    }
}
