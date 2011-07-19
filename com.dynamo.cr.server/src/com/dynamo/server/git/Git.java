package com.dynamo.server.git;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.StringReader;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.Set;
import java.util.StringTokenizer;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.protocol.proto.Protocol.CommitDesc;
import com.dynamo.cr.protocol.proto.Protocol.Log;
import com.dynamo.server.git.CommandUtil.Result;
import com.dynamo.server.git.GitStatus.Entry;

public class Git {

    protected static Logger logger = LoggerFactory.getLogger(Git.class);

    public Git() {
    }

    public static boolean checkGitVersion() {
        try {
            CommandUtil.Result res = execGitCommand(null, "git", "--version");
            if (res.exitValue != 0) {
                logger.warn("Error running git --version.");
                return false;
            }
            else {
                if (!res.stdOut.toString().startsWith("git version 1.7")) {
                    logger.warn("Unsupported git version '" + res.stdOut.toString() + "'");
                    return false;
                }
                else {
                    return true;
                }
            }
        } catch (IOException e) {
            logger.warn("Unable to run git --version", e);
            return false;
        }
    }

    static CommandUtil.Result execGitCommand(String working_dir, String... command) throws IOException {
        return CommandUtil.execCommand(working_dir, null, command);
    }

    static void checkResult(CommandUtil.Result r) throws GitException {
        checkResult(r, 0);
    }

    static void checkResult(CommandUtil.Result r, int max_error) throws GitException {
        if (r.exitValue > max_error) {
            throw new GitException(r.stdErr.toString() + "\n" + r.stdOut.toString());
        }
    }

    // We have two regexps due to problem with parsing "->" and non-greedy parsing.
    // It is perhaps possible to write in a single regexp but this seems to work.
    static Pattern statusPattern1 = Pattern.compile("([MADRCU ])([MADU ])[ ](.*?)( -> )(.*?)?$");
    static Pattern statusPattern2 = Pattern.compile("([MADRCU ])([MADU ])[ ](.*)$");

    static GitStatus.Entry parseStatusEntry(String line) throws GitException {
        Matcher m1 = statusPattern1.matcher(line);
        Matcher m2 = statusPattern2.matcher(line);
        if (m1.matches()) {
            GitStatus.Entry entry = new GitStatus.Entry(m1.group(1).charAt(0), m1.group(2).charAt(0), m1.group(5));
            entry.original = m1.group(3);
            return entry;
        }
        else if (m2.matches()) {
            GitStatus.Entry entry = new GitStatus.Entry(m2.group(1).charAt(0), m2.group(2).charAt(0), m2.group(3));
            entry.original = null;
            return entry;
        }
        else {
            throw new GitException("Unable to parse status string: " + line);
        }
    }

    static GitStatus parseStatus(String status) throws IOException {
        BufferedReader r = new BufferedReader(new StringReader(status));
        String line = r.readLine();

        GitStatus git_status = new GitStatus(0, 0);

        while (line != null) {
            git_status.files.add(parseStatusEntry(line));
            line = r.readLine();
        }

        return git_status;
    }

    /**
     * Clone Git repository (git clone repository directory)
     * @param repository Repository to clone
     * @param directory Directory to cloned
     * @throws IOException
     */
    public void cloneRepo(String repository, String directory) throws IOException {
        CommandUtil.Result r = execGitCommand(null, "git", "clone", repository, directory);
        checkResult(r);
    }

    /**
     * Clone Git repository bare (git clone repository directory). core.sharedRepository is set to group. Permissions is set to "g+ws"
     * @param repository Repository to clone
     * @param directory Directory to cloned
     * @param group Group to set for the repository, ie UNIX-group. Use null for default group.
     * @throws IOException
     */
    public void cloneRepoBare(String repository, String directory, String group) throws IOException {
        CommandUtil.Result r = execGitCommand(null, "git", "clone", "--bare", "--no-hardlinks", repository, directory);
        checkResult(r);

        r = execGitCommand(directory, "git", "repo-config", "core.sharedRepository", "group");
        checkResult(r);

        r = execGitCommand(null, "chmod", "-R", "g+ws", directory);
        checkResult(r);

        if (group != null) {
            r = execGitCommand(null, "chgrp", "-R", group, directory);
            checkResult(r);
        }
    }

    /**
     * Pull and merge changes. (git pull)
     * @note Only valid if is GitState.CLEAN state
     * @param directory Git directory
     * @return True on success. False if merge conflicts
     * @throws IOException
     */
    public boolean pull(String directory) throws IOException {
        if (getState(directory) != GitState.CLEAN) {
            throw new GitException("Pull if only valid if repository is in clean state");
        }

        CommandUtil.Result r = execGitCommand(directory, "git", "pull");
        checkResult(r, 1);
        return r.exitValue == 0;
    }

    /**
     * Get state. (git status)
     * @param directory Directory
     * @return GitState
     * @throws IOException
     */
    public GitState getState(String directory) throws IOException {
        CommandUtil.Result r = execGitCommand(directory, "git", "status", "--porcelain");
        checkResult(r);

        GitStatus status = parseStatus(r.stdOut.toString());

        if (new File(directory, ".git/MERGE_HEAD").exists())
            return GitState.MERGE;

        if (status.files.size() == 0) {
            return GitState.CLEAN;
        }
        else {
            return GitState.DIRTY;
        }
    }

    /**
     * Get status. (git status)
     * @note Slightly different behaviour from git status. When in merge mode
     * files resolved as "yours" are present in the files list even though we file
     * does not differ. This behaviour is required for graphical merge tools. Modified and deleted
     * files are supported.
     * @param directory Directory
     * @return GitStatus
     * @throws IOException
     */
    public GitStatus getStatus(String directory) throws IOException {
        CommandUtil.Result r = execGitCommand(directory, "git", "status", "--porcelain");
        checkResult(r);

        GitStatus status = parseStatus(r.stdOut.toString());
        status.commitsAhead = commitsAhead(directory);
        status.commitsBehind = commitsBehind(directory);

        File merge_head = new File(directory, ".git/MERGE_HEAD");

        if (merge_head.exists()) {
            // We are in merge mode. Code below is a work-around for default git behaviour
            // Files resolved as "yours" will not appear in git status after add.
            // We return such files as modified 'M' or deleted 'D' even though that they are not, per se,
            // modified or deleted. This behaviour is required for our merge tool.
            // Otherwise files would disappear after "resolve yours"

            // Set of all file-names from git status
            Set<String> status_names = new HashSet<String>();
            for (GitStatus.Entry e : status.files) {
                status_names.add(e.file);
            }

            // Get sha1 for merge-head
            BufferedReader reader = new BufferedReader(new FileReader(merge_head));
            String merge_head_sha1 = reader.readLine().trim();
            reader.close();

            // Get list of all files differing
            r = execGitCommand(directory, "git", "diff", "--name-only", String.format("%s..", merge_head_sha1));
            checkResult(r);

            StringTokenizer tokenizer = new StringTokenizer(r.stdOut.toString(), "\n");
            while (tokenizer.hasMoreTokens()) {
                String file_name = tokenizer.nextToken();
                if (!status_names.contains(file_name)) {
                    // Add only files not in "git status"-list to files
                    char index_status;
                    if (new File(directory, file_name).exists())
                        index_status = 'M';
                    else
                        index_status = 'D';
                    GitStatus.Entry new_e = new GitStatus.Entry(index_status, ' ', file_name);
                    status.files.add(new_e);
                }
            }
        }

        // Sort the list. We always wan't to keep the order same regardless
        // if the file came for git status or git diff
        Collections.sort(status.files, new Comparator<GitStatus.Entry>() {
            @Override
            public int compare(Entry o1, Entry o2) {
                return o1.file.compareTo(o2.file);
            }
        });

        return status;
    }

    /**
     * Commit all. (git commit -a -m message)
     * @param directory Directory
     * @param message Commit message
     * @throws IOException
     */
    public CommitDesc commitAll(String directory, String message) throws IOException {
        if (getState(directory) == GitState.MERGE) {
            throw new GitException("commitAll is not permited when repository is in merge state. Resolve conflicts and use commit() instead.");
        }

        CommandUtil.Result r = execGitCommand(directory, "git", "commit", "-a", "-m", message);
        checkResult(r);
        Log log = log(directory, 1);
        return log.getCommits(0);
    }

    /**
     * Commit. (git commit -m message)
     * @param directory Directory
     * @param message Commit message
     * @throws IOException
     */
    public CommitDesc commit(String directory, String message) throws IOException {
        CommandUtil.Result r = execGitCommand(directory, "git", "commit", "-m", message);
        checkResult(r);
        Log log = log(directory, 1);
        return log.getCommits(0);
    }

    /**
     * Simple conflict resolve. (git checkout-index --stage=X -f FILE + git add FILE)
     * @note Must be in GitState.MERGE
     * @param directory Directory
     * @param file File to resolve
     * @param stage Resolve to stage
     * @throws IOException
     */
    public void resolve(String directory, String file, GitStage stage) throws IOException {
        if (getState(directory) != GitState.MERGE) {
            throw new GitException("resolve() is only valid operation when in MERGE state");
        }

        // Uee git checkout {--ours|--theirs} instead?

        CommandUtil.Result r;

        // git could print errors here (if file is UU) but still returns 0
        // This operation is required "resolve()" is called more than once
        // Otherwise checkout-index doesn't work.
        r = execGitCommand(directory, "git", "update-index", "--unresolve", file);
        checkResult(r);

        r = execGitCommand(directory, "git", "status", "--porcelain", file);
        checkResult(r);

        GitStatus status = parseStatus(r.stdOut.toString());
        GitStatus.Entry entry = status.files.get(0);
        if (!entry.file.equals(file))
            throw new GitException("Internal error.");


        if (entry.workingTreeStatus == 'D' && stage == GitStage.THEIRS) {
            // Deleted by them
            r = execGitCommand(directory, "git", "rm", file);
            checkResult(r);
        }
        else if (entry.indexStatus == 'D' && stage == GitStage.YOURS) {
            // Deleted by us
            r = execGitCommand(directory, "git", "rm", file);
            checkResult(r);
        }
        else {
            // Default behaviour. Checkout file from index.
            r = execGitCommand(directory, "git", "checkout-index", "--stage=" + stage.stage, "-f", file);
            checkResult(r);

            r = execGitCommand(directory, "git", "add", file);
            checkResult(r);
        }
    }

    /**
     * Push. (git push)
     * @param directory Directory
     * @throws IOException
     */
    public void push(String directory) throws IOException {
        CommandUtil.Result r = execGitCommand(directory, "git", "push");
        checkResult(r);
    }

    /**
     * Add file (git add)
     * @param directory Directory
     * @param file File to add
     * @throws IOException
     */
    public void add(String directory, String file) throws IOException {
        CommandUtil.Result r = execGitCommand(directory, "git", "add", file);
        checkResult(r);
    }

    /**
     * Number of commits ahead origin/master
     * @param directory Directory
     * @return Number of commits ahead of origin/master
     * @throws IOException
     */
    public int commitsAhead(String directory) throws IOException {
        CommandUtil.Result r;
        r = execGitCommand(directory, "git", "fetch");
        checkResult(r);

        r = execGitCommand(directory, "git", "log", "--oneline", "origin/master..");
        checkResult(r);
        StringTokenizer tokenizer = new StringTokenizer(r.stdOut.toString(), "\n");
        return tokenizer.countTokens();
    }

    /**
     * Number of commits behind origin/master
     * @param directory Directory
     * @return Number of commits ahead of origin/master
     * @throws IOException
     */
    public int commitsBehind(String directory) throws IOException {
        CommandUtil.Result r;
        r = execGitCommand(directory, "git", "fetch");
        checkResult(r);

        r = execGitCommand(directory, "git", "log", "--oneline", "..origin/master");
        checkResult(r);
        StringTokenizer tokenizer = new StringTokenizer(r.stdOut.toString(), "\n");
        return tokenizer.countTokens();
    }

    /**
     * Remove file
     * @param directory Directory
     * @param file File to remove
     * @param force Force delete (-f)
     * @throws IOException
     */
    public void rm(String directory, String file, boolean recursive, boolean force) throws IOException {
        CommandUtil.Result r;
        if (force)
            if (recursive)
                r = execGitCommand(directory, "git", "rm", "-f", "-r", "--ignore-unmatch", file);
            else
                r = execGitCommand(directory, "git", "rm", "-f", "--ignore-unmatch", file);
        else
            if (recursive)
                r = execGitCommand(directory, "git", "rm", "-r", file);
            else
                r = execGitCommand(directory, "git", "rm", file);
        checkResult(r);
    }

    /**
     * Move or rename resource
     * @param directory Directory
     * @param source Source resource
     * @param destination Destination resource
     * @param force Force delete (-f)
     * @throws IOException
     */
    public void mv(String directory, String source, String destination, boolean force) throws IOException {
        CommandUtil.Result r;
        if (force)
            r = execGitCommand(directory, "git", "mv", "-f", source, destination);
        else
            r = execGitCommand(directory, "git", "mv", source, destination);
        checkResult(r);
    }

    /**
     * Checkout resource from repository
     * @param directory Directory of the repository
     * @param file Path of the resource to checkout, relative repository
     * @param force Continue even if the resource is unmerged
     * @throws IOException
     */
    public void checkout(String directory, String file, boolean force) throws IOException {
        CommandUtil.Result r;
        if (force)
            r = execGitCommand(directory, "git", "checkout", "-f", file);
        else
            r = execGitCommand(directory, "git", "checkout", file);
        checkResult(r);
    }

    /**
     * Resets the index to the specified target version.
     * @param directory Directory of the repository
     * @param file Path of the resource to reset, relative repository. null means the whole tree will be reset.
     * @param target Target version to reset to
     * @throws IOException
     */
    public void reset(String directory, GitResetMode resetMode, String file, String target) throws IOException {
        CommandUtil.Result r;
        String reset;
        switch (resetMode) {
        case SOFT:
            reset = "--soft";
            break;
        case MIXED:
            reset = "--mixed";
            break;
        case HARD:
            reset = "--hard";
            break;
        case MERGE:
            reset = "--merge";
            break;
        case KEEP:
            reset = "--keep";
            break;
        default:
            throw new GitException("Reset mode " + resetMode + " not supported.");
        }
        if (file != null) {
            r = execGitCommand(directory, "git", "reset", reset, "-q", target, "--", file);
        } else {
            r = execGitCommand(directory, "git", "reset", reset, "-q", target);
        }
        checkResult(r);
    }

    /**
     * Create and initialize a new git repository. core.sharedRepository is set to group. Permissions is set to "g+ws"
     * @param path path to where the repository should be created
     * @param group Group to set for the repository, ie UNIX-group. Use null for default group.
     */
    public void initBare(String path, String group) throws IOException {
        Result r = execGitCommand(path, "git", "init", "--bare");
        checkResult(r);

        r = execGitCommand(path, "git", "repo-config", "core.sharedRepository", "group");
        checkResult(r);

        r = execGitCommand(null, "chmod", "-R", "g+ws", path);
        checkResult(r);

        if (group != null) {
            r = execGitCommand(null, "chgrp", "-R", group, path);
            checkResult(r);
        }
    }

    /**
     * Removes a git repo from the file system. Use with caution since no branches or clones are removed.
     * @param path path of the repository
     */
    public void rmRepo(String path) throws IOException {
        Result r = CommandUtil.execCommand(new String[] {"rm", "-rf", path});
        checkResult(r);
    }

    /**
     * Returns the content of the file at the specified revision, runs:
     * git show {revision}:{file}
     * @param directory Repository root
     * @param file File to be read
     * @param revision File revision
     * @return File content
     */
    public byte[] show(String directory, String file, String revision) throws IOException {
        Result r = execGitCommand(directory, "git", "show", String.format("%s:%s", revision, file));
        checkResult(r);
        return r.stdOut.toString().getBytes();
    }

    /**
     * Returns an array of performed commits on the branch at the specified directory with a specified max count. The array is ordered from newest to oldest.
     * git log --pretty=oneline
     * @param directory Repository root
     * @param maxCount Maximum number of commits
     * @return Log containing commit messages
     */
    public Log log(String directory, int maxCount) throws IOException {
        Result r = execGitCommand(directory, "git", "log", "--pretty=oneline");
        checkResult(r);
        Log.Builder logBuilder = Log.newBuilder();
        BufferedReader reader = new BufferedReader(new StringReader(r.stdOut.toString()));
        String line;
        int index = 0;
        while (index < maxCount && (line = reader.readLine()) != null) {
            line = line.trim();
            int sep = line.indexOf(' ');
            String id = line.substring(0, sep);
            String message = line.substring(sep + 1);
            logBuilder.addCommits(CommitDesc.newBuilder().setId(id).setMessage(message).build());
            ++index;
        }
        return logBuilder.build();
    }
}
