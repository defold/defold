package com.dynamo.cr.branchrepo;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.regex.Pattern;

import javax.ws.rs.core.Response.Status;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.protocol.proto.Protocol.BranchList;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.CommitDesc;
import com.dynamo.cr.protocol.proto.Protocol.Log;
import com.dynamo.cr.protocol.proto.Protocol.ResolveStage;
import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo.Builder;
import com.dynamo.server.dgit.GitFactory;
import com.dynamo.server.dgit.GitFactory.Type;
import com.dynamo.server.dgit.GitResetMode;
import com.dynamo.server.dgit.GitStage;
import com.dynamo.server.dgit.GitState;
import com.dynamo.server.dgit.GitStatus;
import com.dynamo.server.dgit.GitStatus.Entry;
import com.dynamo.server.dgit.IGit;

public abstract class BranchRepository {

    private Type gitType;
    private String branchRoot;
    private String repositoryRoot;
    private String builtinsDirectory;
    private Pattern[] filterPatterns;

    // Stats
    private AtomicInteger resourceDataRequests = new AtomicInteger();
    private AtomicInteger resourceInfoRequests = new AtomicInteger();

    private String email;
    private String password;
    private String host;

    public BranchRepository(GitFactory.Type gitType,
                            String branchRoot,
                            String repositoryRoot,
                            String builtinsDirectory,
                            Pattern[] filterPatterns,
                            String email,
                            String password,
                            String repositoryHost) {
        this.gitType = gitType;
        this.branchRoot = branchRoot;
        this.repositoryRoot = repositoryRoot;
        this.builtinsDirectory = builtinsDirectory;
        this.filterPatterns = filterPatterns;
        this.email = email;
        this.password = password;
        this.host = repositoryHost;
    }

    public int getResourceDataRequests() {
        return resourceDataRequests.get();
    }

    public int getResourceInfoRequests() {
        return resourceInfoRequests.get();
    }

    public String getBranchRoot() {
        return this.branchRoot;
    }

    protected abstract void ensureProject(String project);
    protected abstract String getFullName(String userId);
    protected abstract String getEmail(String userId);

    void ensureProjectBranch(String project, String user, String branch) throws BranchRepositoryException {
        // We check that the project really exists here
        ensureProject(project);

        long userId = Long.parseLong(user);
        String p = String.format("%s/%s/%d/%s", branchRoot, project, userId, branch);
        if (!new File(p).exists()) {
            throw new BranchRepositoryException(String.format("No such branch %s/%s", user, branch));
        }
    }

    private IGit getGit() {
        IGit git = GitFactory.create(this.gitType);
        if (email != null && password != null) {
            git.setUsername(email);
            git.setPassword(password);
            git.setHost(host);
        }
        return git;
    }

    public void createBranch(String project, String user, String branch) throws BranchRepositoryException, IOException {
        // We check that the project really exists here
        ensureProject(project);
        long userId = Long.parseLong(user);

        String p = String.format("%s/%s/%d/%s", branchRoot, project, userId, branch);
        File f = new File(p);
        if (f.exists()) {
            throw new BranchRepositoryException(String.format("Branch %s already exists", p));
        }

        f.getParentFile().mkdirs();

        IGit git = getGit();
        String sourcePath = String.format("%s/%s", this.repositoryRoot, project);
        git.cloneRepo(sourcePath, p);

        String fullName = getFullName(user);
        String email = getEmail(user);

        git.configUser(p, email, fullName);

        if (this.builtinsDirectory != null) {
            String dest = String.format("%s/builtins", p);
            // Copy builtins into the branch. See deleteBranch()
            // The reason for copying is that mklink on windows is really broken (privileges).
            try {
                FileUtils.copyDirectory(new File(builtinsDirectory), new File(dest));
            } catch (IOException e) {
                FileUtils.deleteDirectory(new File(p));
                throw new BranchRepositoryException(String.format("Unable to create branch. Internal server error"));
            }
        }
    }

    public void deleteBranch(String project, String user, String branch) throws BranchRepositoryException  {
        ensureProjectBranch(project, user, branch);
        long userId = Long.parseLong(user);

        String p = String.format("%s/%s/%d/%s", branchRoot, project, userId, branch);
        try {
            FileUtils.deleteDirectory(new File(p));
        } catch (IOException e) {
            throw new BranchRepositoryException("", e);
        }
    }

    public Protocol.ResourceInfo getResourceInfo(String project, String user, String branch,
            String path) throws IOException, BranchRepositoryException {
        ensureProjectBranch(project, user, branch);
        if (path == null)
            throw new BranchRepositoryException("null path");

        if (path.indexOf("..") != -1) {
            throw new BranchRepositoryException("Relative paths are not permitted", Status.NOT_FOUND);
        }

        resourceInfoRequests.addAndGet(1);

        long userId = Long.parseLong(user);
        String p = String.format("%s/%s/%d/%s%s", branchRoot, project, userId, branch, path);
        File f = new File(p);

        if (!f.exists())
            throw new BranchRepositoryException(String.format("%s not found", p), Status.NOT_FOUND);

        if (f.isFile()) {
            Builder builder = Protocol.ResourceInfo.newBuilder()
            .setName(f.getName())
            .setPath(String.format("%s", path))
            .setSize((int) f.length())
            .setLastModified(f.lastModified())
            .setType(Protocol.ResourceType.FILE);

            return builder.build();
        }
        else if (f.isDirectory()) {
            Builder builder = Protocol.ResourceInfo.newBuilder()
            .setName(f.getName())
            .setPath(String.format("%s", path))
            .setSize(0)
            .setLastModified(f.lastModified())
            .setType(Protocol.ResourceType.DIRECTORY);

            for (File subf : f.listFiles()) {
                boolean match = false;
                String pathToMatch = path;
                if (pathToMatch.equals("/"))
                    pathToMatch += subf.getName();
                else
                    pathToMatch += "/" + subf.getName();

                for (Pattern pf : filterPatterns) {
                    if (pf.matcher(pathToMatch).matches()) {
                        match = true;
                        break;
                    }
                }
                if (!match)
                    builder.addSubResourceNames(subf.getName());
            }
            return builder.build();
        }
        else {
            throw new BranchRepositoryException("Unknown resource type: " + f);
        }
    }

    public byte[] getResourceData(String project, String user, String branch,
            String path, String revision) throws IOException, BranchRepositoryException {
        ensureProjectBranch(project, user, branch);
        if (path == null)
            throw new BranchRepositoryException("null path");

        if (path.indexOf("..") != -1) {
            throw new BranchRepositoryException("Relative paths are not permitted", Status.NOT_FOUND);
        }

        resourceDataRequests.addAndGet(1);

        long userId = Long.parseLong(user);
        String p = String.format("%s/%s/%d/%s%s", branchRoot, project, userId, branch, path);
        if (revision != null && !revision.equals("")) {
            IGit git = getGit();
            try {
                return git.show(String.format("%s/%s/%d/%s", branchRoot, project, userId, branch), path.substring(1), revision);
            } catch (IOException e) {
                throw new BranchRepositoryException(String.format("%s (rev '%s') not found", p, revision), Status.NOT_FOUND);
            }
        } else {
            File f = new File(p);

            if (!f.exists())
                throw new BranchRepositoryException(String.format("%s not found", p), Status.NOT_FOUND);
            else if (!f.isFile())
                throw new BranchRepositoryException("getResourceData opertion is not applicable on non-files");

            byte[] file_data = new byte[(int) f.length()];
            BufferedInputStream is = new BufferedInputStream(new FileInputStream(p));
            is.read(file_data);
            is.close();
            return file_data;
        }
    }

    public void deleteResource(String project, String user, String branch,
            String path) throws BranchRepositoryException, IOException {
        ensureProjectBranch(project, user, branch);
        if (path == null)
            throw new BranchRepositoryException("null path");
        long userId = Long.parseLong(user);

        String p = String.format("%s/%s/%d/%s%s", branchRoot, project, userId, branch, path);
        File f = new File(p);
        // NOTE: We need to cache isFile() here.
        boolean isFile = f.isFile();

        if (!f.exists())
            throw new BranchRepositoryException(String.format("%s not found", p), Status.NOT_FOUND);

        boolean recursive = false;
        if (f.isDirectory())
            recursive = true;

        String branch_path = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
        IGit git = getGit();
        boolean force = true;
        git.rm(branch_path, path.substring(1), recursive, force); // NOTE: Remove / from path

        // The code is somewhat strange
        // We always recreate to parent directory for a file
        // in order to avoid implicit deletion of directories. This behavior confuses
        // eclipse. We also need to remove the directory manually as git won't remove
        // empty directories. (git doesn't track empty directories). This is direct
        // consequence of the first behavior (recreating directories)
        if (isFile) {
            String localPath = FilenameUtils.getFullPath(p);
            File localPathFile = new File(localPath);
            localPathFile.mkdirs();
        } else {
            f.delete();
        }
    }

    public void renameResource(String project, String user, String branch,
            String source, String destination) throws BranchRepositoryException, IOException {
        ensureProjectBranch(project, user, branch);
        if (source == null)
            throw new BranchRepositoryException("null source");

        long userId = Long.parseLong(user);
        String source_path = String.format("%s/%s/%d/%s%s", branchRoot, project, userId, branch, source);
        File f = new File(source_path);

        if (!f.exists())
            throw new BranchRepositoryException(String.format("%s not found", source_path), Status.NOT_FOUND);

        String branch_path = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
        IGit git = getGit();
        git.mv(branch_path, source.substring(1), destination.substring(1), true); // NOTE: Remove / from path
    }

    public void revertResource(String project, String user, String branch,
            String path) throws IOException, BranchRepositoryException {
        long userId = Long.parseLong(user);
        String branch_path = String.format("%s/%s/%d/%s", branchRoot, project, userId, branch);
        IGit git = getGit();
        GitStatus status = git.getStatus(branch_path, false);
        String localPath = path.substring(1);
        GitStatus.Entry entry = null;
        for (GitStatus.Entry f : status.files) {
            if (f.file.equals(path.substring(1))) {
                entry = f;
                break;
            }
        }
        if (entry != null) {
            switch (entry.indexStatus) {
            case 'A':
                git.rm(branch_path, localPath, false, true);
                break;
            case 'R':
                git.mv(branch_path, localPath, entry.original, false);
                git.reset(branch_path, GitResetMode.MIXED, entry.original, "HEAD");
                git.checkout(branch_path, entry.original, false);
                /*
                    NOTE: Previous code. Could get it to work in JGit
                    Perhaps some discrepancy? I leave the note and code
                    here in case any problems

                    git.rm(branch_path, localPath, false, true);
                    git.reset(branch_path, GitResetMode.MIXED, entry.original, "HEAD");
                    git.checkout(branch_path, entry.original, false);
                */
                break;
            case 'D':
            case 'M':
                git.reset(branch_path, GitResetMode.MIXED, localPath, "HEAD");
            default:
                git.checkout(branch_path, localPath, false);
                break;
            }
        }
    }

    public void putResourceData(String project, String user, String branch,
            String path, byte[] data) throws BranchRepositoryException, IOException  {
        ensureProjectBranch(project, user, branch);
        if (path == null)
            throw new BranchRepositoryException("null path");

        String p = String.format("%s/%s/%s/%s%s", branchRoot, project, user, branch, path);
        File f = new File(p);
        File parent = f.getParentFile();

        if (!parent.exists())
            throw new BranchRepositoryException(String.format("Directory %s not found", parent.getPath()), Status.NOT_FOUND);

        FileOutputStream os = new FileOutputStream(f);
        os.write(data);
        os.close();

        String branch_path = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
        IGit git = getGit();
        try {
            git.add(branch_path, path.substring(1));  // NOTE: Remove / from path
        } catch (Throwable e) {
            // Rollback if git error
            f.delete();
            throw new BranchRepositoryException("Failed to add file", e);
        }
    }

    public void mkdir(String project, String user, String branch, String path) throws BranchRepositoryException {
        if (path == null)
            throw new BranchRepositoryException("null path");

        String p = String.format("%s/%s/%s/%s%s", branchRoot, project, user, branch, path);
        File f = new File(p);
        if (!f.exists()) {
            f.mkdirs();
        }
        else {
            if (!f.isDirectory())
                throw new BranchRepositoryException(String.format("File %s already exists and is not a directory", p));
        }
    }

    public BranchStatus getBranchStatus(String project, String user, String branch, boolean fetch) throws IOException, BranchRepositoryException {
        ensureProjectBranch(project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        IGit git = getGit();
        GitState state = git.getState(p);
        GitStatus status = git.getStatus(p, fetch);

        Protocol.BranchStatus.State s;
        if (state == GitState.CLEAN)
            s = Protocol.BranchStatus.State.CLEAN;
        else if (state == GitState.DIRTY)
            s = Protocol.BranchStatus.State.DIRTY;
        else if (state == GitState.MERGE)
            s = Protocol.BranchStatus.State.MERGE;
        else
            throw new RuntimeException("Internal error. Unknown state: " + state);

        BranchStatus.Builder builder = Protocol.BranchStatus
        .newBuilder().setName(branch)
        .setBranchState(s)
        .setCommitsAhead(status.commitsAhead)
        .setCommitsBehind(status.commitsBehind);

        for (GitStatus.Entry e : status.files) {
            String fn = String.format("/%s", e.file);
            BranchStatus.Status.Builder status_builder = BranchStatus.Status.newBuilder().setName(fn).setIndexStatus("" + e.indexStatus).setWorkingTreeStatus("" + e.workingTreeStatus);
            if (e.original != null) {
                status_builder.setOriginal(String.format("/%s", e.original));
            }
            builder.addFileStatus(status_builder);
        }

        return builder.build();
    }

    public void autoStage(String project, String user, String branch) throws BranchRepositoryException, IOException {
        ensureProjectBranch(project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
        IGit git = getGit();
        GitStatus status = git.getStatus(p, false);

        for (Entry f : status.files) {
            char ws = f.workingTreeStatus;
            if (ws == '?' || ws == 'M') {
                System.out.println("autostaging: " + f.file);
                git.add(p, f.file);
            } else if (ws == 'D') {
                if (f.indexStatus == 'A') {
                    git.reset(p, GitResetMode.MIXED, f.file, "HEAD");
                } else {
                    git.rm(p, f.file, true, true);
                }
            }
        }
    }

    public String[] getBranchNames(String project, String user) {
        String p = String.format("%s/%s/%s", branchRoot, project, user);
        File d = new File(p);
        if (!d.isDirectory())
            return new String[] {};

        List<String> list = new ArrayList<String>();
        for (File f : d.listFiles()) {
            if (f.isDirectory()) {
                if (new File(f, ".git").isDirectory()) {
                    list.add(f.getName());
                }
            }
        }

        String[] ret = new String[list.size()];
        list.toArray(ret);

        return ret;
    }

    public BranchList getBranchList(String project, String user) {
        String[] branchNames = getBranchNames(project, user);
        Protocol.BranchList.Builder b = Protocol.BranchList.newBuilder();
        for (String branch : branchNames) {
            b.addBranches(branch);
        }
        return b.build();
    }

    public void updateBranch(String project, String user, String branch) throws IOException, BranchRepositoryException {
        ensureProjectBranch(project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        IGit git = getGit();
        git.pull(p);
    }

    public CommitDesc commitBranch(String project, String user, String branch, String message) throws IOException, BranchRepositoryException {
        ensureProjectBranch(project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        IGit git = getGit();
        return git.commitAll(p, message);
    }

    public void resolveResource(String project, String user, String branch, String path,
            ResolveStage stage) throws IOException, BranchRepositoryException {
        ensureProjectBranch(project, user, branch);
        if (path == null)
            throw new BranchRepositoryException("null path");

        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        IGit git = getGit();
        GitStage git_stage;
        if (stage == ResolveStage.BASE) {
            git_stage = GitStage.BASE;
        }
        else if (stage == ResolveStage.YOURS) {
            git_stage = GitStage.YOURS;
        }
        else if (stage == ResolveStage.THEIRS) {
            git_stage = GitStage.THEIRS;
        }
        else {
            throw new RuntimeException("Internal error. Unknown stage: " + stage);
        }

        git.resolve(p, path.substring(1), git_stage);  // NOTE: Remove / from path
    }

    public CommitDesc commitMergeBranch(String project, String user, String branch,
            String message) throws IOException, BranchRepositoryException {
        ensureProjectBranch(project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        IGit git = getGit();
        return git.commit(p, message);
    }

    public void publishBranch(String project, String user, String branch) throws IOException, BranchRepositoryException {
        ensureProjectBranch(project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        IGit git = getGit();
        git.push(p);
    }

    public void reset(String project, String user, String branch, String mode, String target) throws IOException, BranchRepositoryException {
        ensureProjectBranch(project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        IGit git = getGit();
        GitResetMode resetMode = GitResetMode.MIXED;
        if (mode.equals("mixed")) {
            resetMode = GitResetMode.MIXED;
        } else if (mode.equals("soft")) {
            resetMode = GitResetMode.SOFT;
        } else if (mode.equals("hard")) {
            resetMode = GitResetMode.HARD;
        } else if (mode.equals("merge")) {
            resetMode = GitResetMode.MERGE;
        } else if (mode.equals("keep")) {
            resetMode = GitResetMode.KEEP;
        }
        git.reset(p, resetMode, null, target);
    }

    public Log logBranch(String project, String user, String branch, int maxCount) throws IOException, BranchRepositoryException {
        ensureProjectBranch(project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        IGit git = getGit();
        return git.log(p, maxCount);
    }

}
