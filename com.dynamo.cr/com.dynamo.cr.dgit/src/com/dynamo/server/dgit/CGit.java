package com.dynamo.server.dgit;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.StringReader;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.StringTokenizer;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.protocol.proto.Protocol.CommitDesc;
import com.dynamo.cr.protocol.proto.Protocol.Log;
import com.dynamo.server.dgit.CommandUtil.Result;
import com.dynamo.server.dgit.GitStatus.Entry;

public class CGit implements IGit {

    private String username;
    private String password;
    private String host;

    protected static Logger logger = LoggerFactory.getLogger(CGit.class);

    public CGit() {
    }

    @Override
    public boolean checkGitVersion() {
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

    private void updateNetRC(File netRC) throws IOException {
        FileWriter fw = null;
        try {
            fw = new FileWriter(netRC);
            fw.write(String.format("machine %s login %s password %s", this.host, this.username, this.password));
        } catch (IOException e) {
            throw e;
        } finally {
            if (fw != null) {
                fw.close();
            }
        }
    }

    private CommandUtil.Result execGitCommand(String working_dir, String... command) throws IOException {
        Map<String, String> env = new HashMap<String, String>();
        // fix netrc file
        if (this.host != null) {
            File netRC = DGit.getNetRC();
            updateNetRC(netRC);
            // Redirect home in env
            env.put("HOME", netRC.getParent());
        }

        String gitDir = DGit.getDefault().getGitDir();
        String gitBinDir = String.format("%s/bin/", gitDir);
        String gitExecPath = String.format(gitDir + "/libexec/git-core");
        if (!DGit.getPlatform().contains("win32")) {
            env.put("GIT_EXEC_PATH", gitExecPath);
        }
        // Prepend git path
        command[0] = gitBinDir + command[0];
        return CommandUtil.execCommand(working_dir, null, command, env);
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
    static Pattern statusPattern1 = Pattern.compile("([MADRCU\\? ])([MADU\\? ])[ ]\"?(.*?)\"?( -> )\"?(.*?)?\"?$");
    static Pattern statusPattern2 = Pattern.compile("([MADRCU\\? ])([MADU\\? ])[ ]\"?(.*?)\"?$");

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

    @Override
    public void cloneRepo(String repository, String directory) throws IOException {
        CommandUtil.Result r = execGitCommand(null, "git", "clone", repository, directory);
        checkResult(r);
    }

    @Override
    public void cloneRepoBare(String repository, String directory, String group) throws IOException {
        CommandUtil.Result r = execGitCommand(null, "git", "clone", "--bare", "--no-hardlinks", repository, directory);
        checkResult(r);

        r = execGitCommand(directory, "git", "repo-config", "core.sharedRepository", "group");
        checkResult(r);

        r = CommandUtil.execCommand(new String[] {"chmod", "-R", "g+ws", directory});
        checkResult(r);

        if (group != null) {
            r = CommandUtil.execCommand(new String[] {"chgrp", "-R", group, directory});
            checkResult(r);
        }
    }

    @Override
    public boolean pull(String directory) throws IOException {
        if (getState(directory) != GitState.CLEAN) {
            throw new GitException("Pull if only valid if repository is in clean state");
        }

        CommandUtil.Result r = execGitCommand(directory, "git", "pull");
        checkResult(r, 1);
        return r.exitValue == 0;
    }

    @Override
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

    @Override
    public GitStatus getStatus(String directory, boolean fetch) throws IOException {
        CommandUtil.Result r = execGitCommand(directory, "git", "status", "--porcelain");
        checkResult(r);

        GitStatus status = parseStatus(r.stdOut.toString());
        status.commitsAhead = commitsAhead(directory, fetch);
        status.commitsBehind = commitsBehind(directory, fetch);

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

    @Override
    public CommitDesc commitAll(String directory, String message) throws IOException {
        if (getState(directory) == GitState.MERGE) {
            throw new GitException("commitAll is not permited when repository is in merge state. Resolve conflicts and use commit() instead.");
        }
        File f = File.createTempFile("commitAll", "message", new File(directory));
        FileUtils.write(f, message);
        CommandUtil.Result r = execGitCommand(directory, "git", "commit", "-a", "-F", f.getName());
        f.delete();
        checkResult(r);
        Log log = log(directory, 1);
        return log.getCommits(0);
    }

    @Override
    public CommitDesc commit(String directory, String message) throws IOException {
        File f = File.createTempFile("commit", "message", new File(directory));
        FileUtils.write(f, message);
        CommandUtil.Result r = execGitCommand(directory, "git", "commit", "-F", f.getName());
        f.delete();
        checkResult(r);
        Log log = log(directory, 1);
        return log.getCommits(0);
    }

    @Override
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

    @Override
    public void push(String directory) throws IOException {
        CommandUtil.Result r = execGitCommand(directory, "git", "push");
        checkResult(r);
    }

    @Override
    public void pushInitial(String directory) throws IOException {
        CommandUtil.Result r = execGitCommand(directory, "git", "push", "origin", "master");
        checkResult(r);
    }

    @Override
    public void add(String directory, String file) throws IOException {
        CommandUtil.Result r = execGitCommand(directory, "git", "add", file);
        checkResult(r);
    }

    @Override
    public int commitsAhead(String directory, boolean fetch) throws IOException {
        CommandUtil.Result r;
        if (fetch) {
            r = execGitCommand(directory, "git", "fetch");
            checkResult(r);
        }

        r = execGitCommand(directory, "git", "log", "--oneline", "origin/master..");
        checkResult(r);
        StringTokenizer tokenizer = new StringTokenizer(r.stdOut.toString(), "\n");
        return tokenizer.countTokens();
    }

    @Override
    public int commitsBehind(String directory, boolean fetch) throws IOException {
        CommandUtil.Result r;
        if (fetch) {
            r = execGitCommand(directory, "git", "fetch");
            checkResult(r);
        }

        r = execGitCommand(directory, "git", "log", "--oneline", "..origin/master");
        checkResult(r);
        StringTokenizer tokenizer = new StringTokenizer(r.stdOut.toString(), "\n");
        return tokenizer.countTokens();
    }

    @Override
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

    @Override
    public void mv(String directory, String source, String destination, boolean force) throws IOException {
        CommandUtil.Result r;
        File sourceFile = new File(directory + "/" + source);
        File destFile = new File(directory + "/" + destination);

        if (sourceFile.isDirectory() && sourceFile.list().length == 0) {
            // Git doesn't track emtpy dirs. Just move the directory
            sourceFile.delete();
            destFile.mkdir();
        } else {
            // Ensure that destination path exists
            String p = FilenameUtils.getPath(destination);
            new File(directory, p).mkdir();

            if (force)
                r = execGitCommand(directory, "git", "mv", "-f", source, destination);
            else
                r = execGitCommand(directory, "git", "mv", source, destination);
            checkResult(r);
        }

    }

    @Override
    public void checkout(String directory, String file, boolean force) throws IOException {
        CommandUtil.Result r;
        if (force)
            r = execGitCommand(directory, "git", "checkout", "-f", file);
        else
            r = execGitCommand(directory, "git", "checkout", file);
        checkResult(r);
    }

    @Override
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

    @Override
    public void initBare(String path, String group) throws IOException {
        Result r = execGitCommand(path, "git", "init", "--bare");
        checkResult(r);

        r = execGitCommand(path, "git", "repo-config", "core.sharedRepository", "group");
        checkResult(r);

        String platform = DGit.getPlatform();
        if (!platform.contains("win32")) {
            r = CommandUtil.execCommand(new String[] {"chmod", "-R", "g+ws", path});
            checkResult(r);

            if (group != null) {
                r = CommandUtil.execCommand(new String[] {"chgrp", "-R", group, path});
                checkResult(r);
            }
        }
    }

    @Override
    public void rmRepo(String path) throws IOException {
        Result r;
        if (DGit.getPlatform().contains("win32")) {
            r = execGitCommand(null, "rm", "-rf", path);
        } else {
            r = CommandUtil.execCommand(new String[] {"rm", "-rf", path});
        }
        checkResult(r);
    }

    @Override
    public byte[] show(String directory, String file, String revision) throws IOException {
        Result r = execGitCommand(directory, "git", "show", String.format("%s:%s", revision, file));
        checkResult(r);
        return r.stdOut.toString().getBytes();
    }

    @Override
    public Log log(String directory, int maxCount) throws IOException {
        Result r = execGitCommand(directory, "git", "log", "--pretty=format:%H%x1F%cn%x1F%ce%x1F%ci%x1F%s");
        checkResult(r);
        Log.Builder logBuilder = Log.newBuilder();
        BufferedReader reader = new BufferedReader(new StringReader(r.stdOut.toString()));
        String line;
        int index = 0;
        while (index < maxCount && (line = reader.readLine()) != null) {
            String[] tokens = line.split("\\x1F");
            String id = tokens[0];
            String name = tokens[1];
            String email = tokens[2];
            String date = tokens[3];
            String message = tokens[4];
            logBuilder.addCommits(CommitDesc.newBuilder().setId(id).setMessage(message).setName(name).setEmail(email).setDate(date).build());
            ++index;
        }
        return logBuilder.build();
    }

    @Override
    public void configUser(String directory, String email, String name) throws IOException {
        String configPath = ".git/config";
        String fileParam = String.format("--file=%s", configPath);
        Result r = execGitCommand(directory, "git", "config", fileParam, "user.email", email);
        checkResult(r);
        r = execGitCommand(directory, "git", "config", fileParam, "user.name", name);
        checkResult(r);
    }

    @Override
    public void config(String directory, String key, String value) throws IOException {
        Result r = execGitCommand(directory, "git", "config", key, value);
        checkResult(r);
    }

    @Override
    public void setUsername(String userName) {
        this.username = userName;
    }

    @Override
    public void setPassword(String passWord) {
        this.password = passWord;
    }

    @Override
    public void setHost(String host) {
        this.host = host;
    }
}
