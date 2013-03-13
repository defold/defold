package com.dynamo.server.dgit;

import static org.eclipse.jgit.lib.Constants.HEAD;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.text.DateFormat;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.eclipse.jgit.api.AddCommand;
import org.eclipse.jgit.api.CheckoutCommand;
import org.eclipse.jgit.api.CloneCommand;
import org.eclipse.jgit.api.CommitCommand;
import org.eclipse.jgit.api.Git;
import org.eclipse.jgit.api.GitCommand;
import org.eclipse.jgit.api.LogCommand;
import org.eclipse.jgit.api.PullCommand;
import org.eclipse.jgit.api.PullResult;
import org.eclipse.jgit.api.PushCommand;
import org.eclipse.jgit.api.ResetCommand;
import org.eclipse.jgit.api.ResetCommand.ResetType;
import org.eclipse.jgit.api.RmCommand;
import org.eclipse.jgit.api.Status;
import org.eclipse.jgit.api.StatusCommand;
import org.eclipse.jgit.api.TransportConfigCallback;
import org.eclipse.jgit.api.errors.InvalidRemoteException;
import org.eclipse.jgit.api.errors.JGitInternalException;
import org.eclipse.jgit.api.errors.NoFilepatternException;
import org.eclipse.jgit.diff.DiffEntry;
import org.eclipse.jgit.diff.DiffEntry.ChangeType;
import org.eclipse.jgit.diff.DiffFormatter;
import org.eclipse.jgit.dircache.DirCache;
import org.eclipse.jgit.dircache.DirCacheBuilder;
import org.eclipse.jgit.dircache.DirCacheEntry;
import org.eclipse.jgit.dircache.DirCacheIterator;
import org.eclipse.jgit.errors.AmbiguousObjectException;
import org.eclipse.jgit.errors.ConfigInvalidException;
import org.eclipse.jgit.errors.IncorrectObjectTypeException;
import org.eclipse.jgit.errors.MissingObjectException;
import org.eclipse.jgit.lib.Constants;
import org.eclipse.jgit.lib.FileMode;
import org.eclipse.jgit.lib.ObjectId;
import org.eclipse.jgit.lib.ObjectLoader;
import org.eclipse.jgit.lib.ObjectReader;
import org.eclipse.jgit.lib.Repository;
import org.eclipse.jgit.revwalk.RevCommit;
import org.eclipse.jgit.revwalk.RevWalk;
import org.eclipse.jgit.storage.file.FileBasedConfig;
import org.eclipse.jgit.storage.file.FileRepositoryBuilder;
import org.eclipse.jgit.transport.PushResult;
import org.eclipse.jgit.transport.RemoteRefUpdate;
import org.eclipse.jgit.transport.Transport;
import org.eclipse.jgit.transport.UsernamePasswordCredentialsProvider;
import org.eclipse.jgit.treewalk.CanonicalTreeParser;
import org.eclipse.jgit.treewalk.FileTreeIterator;
import org.eclipse.jgit.treewalk.TreeWalk;

import com.dynamo.cr.protocol.proto.Protocol.CommitDesc;
import com.dynamo.cr.protocol.proto.Protocol.Log;
import com.dynamo.server.dgit.CommandUtil.Result;
import com.dynamo.server.dgit.GitStatus.Entry;
import com.google.common.collect.ArrayListMultimap;
import com.google.common.collect.Multimap;

public class JGit implements IGit, TransportConfigCallback {

    private String userName;
    private String passWord;

    private Repository buildRepository(String directory) throws IOException {
        FileRepositoryBuilder builder = new FileRepositoryBuilder();
        Repository repo = builder.setGitDir(new File(directory  + "/.git"))
          .readEnvironment() // scan environment GIT_* variables
          .findGitDir() // scan up the file system tree
          .build();
        return repo;
    }

    private Git getGit(String directory) throws IOException {
        Repository repo = buildRepository(directory);
        Git git = new Git(repo);
        return git;
    }

    @Override
    public void configure(Transport transport) {
        if (userName != null && passWord != null) {
            transport.setCredentialsProvider(new UsernamePasswordCredentialsProvider(userName, passWord));
        }
    }

    @Override
    public void cloneRepo(String repository, String directory)
            throws IOException {

        CloneCommand clone = Git.cloneRepository()
                .setURI(repository)
                .setBare(false)
                .setTransportConfigCallback(this)
                .setDirectory(new File(directory));
        clone.call();
    }

    @Override
    public void cloneRepoBare(String repository, String directory, String group)
            throws IOException {
        throw new RuntimeException("Not implemented.");
    }

    @Override
    public boolean pull(String directory) throws IOException {
        if (getState(directory) != GitState.CLEAN) {
            throw new GitException("Pull if only valid if repository is in clean state");
        }
        Git git = getGit(directory);
        PullCommand command = git.pull().setTransportConfigCallback(this);
        try {
            PullResult result = command.call();
            return result.isSuccessful();
        } catch (Exception e) {
            throw new IOException(e);
        }
    }

    @Override
    public GitState getState(String directory) throws IOException {
        if (new File(directory, ".git/MERGE_HEAD").exists())
            return GitState.MERGE;

        Git git = getGit(directory);
        StatusCommand command = git.status();
        Status status = command.call();
        int count = status.getAdded().size()
                  + status.getChanged().size()
                  + status.getConflicting().size()
                  + status.getMissing().size()
                  + status.getModified().size()
                  + status.getRemoved().size()
                  + status.getUntracked().size();

        if (count == 0) {
            return GitState.CLEAN;
        } else {
            return GitState.DIRTY;
        }
    }

    private void processStatus(Multimap<String, GitStatus.Entry> statusMap, List<DiffEntry> files, boolean cached) {
        for (DiffEntry diffE : files) {
            ChangeType changeType = diffE.getChangeType();
            char indexType = 0;
            char workingType = ' ';
            String file = diffE.getNewPath();
            switch (changeType) {
            case ADD:
                indexType = 'A';
                break;
            case COPY:
                indexType = 'C';
                break;
            case DELETE:
                indexType = 'D';
                file = diffE.getOldPath();
                break;
            case MODIFY:
                indexType = 'M';
                break;
            case RENAME:
                indexType = 'R';
                break;
            }
            GitStatus.Entry entry;
            if (cached)
                entry = new GitStatus.Entry(indexType, workingType, file);
            else
                entry = new GitStatus.Entry(workingType, indexType, file);
            entry.original = diffE.getOldPath();
            statusMap.put(file, entry);
        }
    }

    private void getStatusCached(Multimap<String, GitStatus.Entry> statusMap, Repository repo) throws AmbiguousObjectException, IOException {
        DiffFormatter diffFmt = new DiffFormatter(null);
        diffFmt.setRepository(repo);
        diffFmt.setDetectRenames(true);

        ObjectId head = repo.resolve(HEAD + "^{tree}");
        CanonicalTreeParser p = new CanonicalTreeParser();
        ObjectReader reader = repo.newObjectReader();
        try {
            p.reset(reader, head);
        } finally {
            reader.release();
        }
        CanonicalTreeParser oldTree = p;

        DirCacheIterator newTree = new DirCacheIterator(repo.readDirCache());
        List<DiffEntry> files = diffFmt.scan(oldTree, newTree);
        processStatus(statusMap, files, true);
    }

    private void getStatusWorking(Multimap<String, GitStatus.Entry> statusMap, Repository repo) throws AmbiguousObjectException, IOException {
        DiffFormatter diffFmt = new DiffFormatter(null);
        diffFmt.setRepository(repo);
        diffFmt.setDetectRenames(true);

        DirCacheIterator oldTree = new DirCacheIterator(repo.readDirCache());
        FileTreeIterator newTree = new FileTreeIterator(repo);

        List<DiffEntry> files = diffFmt.scan(oldTree, newTree);
        processStatus(statusMap, files, false);
    }

    @Override
    public GitStatus getStatus(String directory, boolean fetch) throws IOException {
        /*
         * NOTE:
         * This method is.. strange..
         * There is not builtin support for "git status". Using the index,
         * DiffFmt etc we try simulate "git status". We should perhaps reimplement
         * this method using low-level jgit instead.
         */
        Git git = getGit(directory);
        Status status = git.status().call();
        Repository repo = git.getRepository();

        Multimap<String, GitStatus.Entry> multiStatusMap = ArrayListMultimap.create();
        getStatusWorking(multiStatusMap, repo);
        getStatusCached(multiStatusMap, repo);

        // Merge status from working and cached...
        Map<String, GitStatus.Entry> statusMap = new HashMap<String, GitStatus.Entry>();
        for (String p : multiStatusMap.keySet()) {
            Entry newEntry = new Entry(' ', ' ', p);
            Collection<Entry> entry = multiStatusMap.get(p);
            for (Entry e : entry) {
                if (e.indexStatus != ' ')
                    newEntry.indexStatus = e.indexStatus;
                if (e.workingTreeStatus != ' ')
                    newEntry.workingTreeStatus = e.workingTreeStatus;
                if (e.original != null)
                    newEntry.original = e.original;
            }
            statusMap.put(p, newEntry);
        }

        // Set status to ?? for all untracked files
        // Yet another hack... :-)
        for (String p : statusMap.keySet()) {
            if (status.getUntracked().contains(p)) {
                Entry e = statusMap.get(p);
                e.indexStatus = '?';
                e.workingTreeStatus = '?';
            }
        }

        Map<String, DirCacheEntry> fullMerged = new HashMap<String, DirCacheEntry>();
        Map<String, DirCacheEntry> base = new HashMap<String, DirCacheEntry>();
        Map<String, DirCacheEntry> ours = new HashMap<String, DirCacheEntry>();
        Map<String, DirCacheEntry> theirs = new HashMap<String, DirCacheEntry>();
        DirCache dirCache = repo.readDirCache();
        int count = dirCache.getEntryCount();
        for (int i = 0; i < count; ++i) {
            DirCacheEntry entry = dirCache.getEntry(i);
            if (entry.getStage() == DirCacheEntry.STAGE_0) {
                fullMerged.put(entry.getPathString(), entry);
            } else if (entry.getStage() == DirCacheEntry.STAGE_1) {
                base.put(entry.getPathString(), entry);
            } else if (entry.getStage() == DirCacheEntry.STAGE_2) {
                ours.put(entry.getPathString(), entry);
            } else if (entry.getStage() == DirCacheEntry.STAGE_3) {
                theirs.put(entry.getPathString(), entry);
            }
        }

        Set<String> allFiles = new HashSet<String>();
        allFiles.addAll(statusMap.keySet());

        for (String file : allFiles) {
            if (status.getConflicting().contains(file)) {

                statusMap.remove(file);

                boolean inBase = base.containsKey(file);
                boolean inTheirs = theirs.containsKey(file);
                boolean inOurs = ours.containsKey(file);

                GitStatus.Entry e = new GitStatus.Entry('U', 'U', file);
                // TODO: Not confident that we catch all types of combinations here
                // We should add more unit-tests to validate this.
                if (inBase && inTheirs && inOurs) {
                    e.workingTreeStatus = 'U';
                }
                else if (inBase && inTheirs && !inOurs) {
                    e.indexStatus = 'D';
                    e.workingTreeStatus = 'U';
                }
                else if (!inTheirs) {
                    if (fullMerged.containsKey(e.file)) {
                        e.workingTreeStatus = ' ';
                    } else {
                        e.workingTreeStatus = 'D';
                    }
                }
                statusMap.put(file, e);
            }
        }

        File merge_head = new File(directory, ".git/MERGE_HEAD");
        if (merge_head.exists()) {
            // We are in merge mode. Code below is a work-around for default git behaviour
            // Files resolved as "yours" will not appear in git status after add.
            // We return such files as modified 'M' or deleted 'D' even though that they are not, per se,
            // modified or deleted. This behaviour is required for our merge tool.
            // Otherwise files would disappear after "resolve yours"

            // Set of all file-names from git status
            Set<String> status_names = statusMap.keySet();

            // Get sha1 for merge-head
            BufferedReader reader = new BufferedReader(new FileReader(merge_head));
            String merge_head_sha1 = reader.readLine().trim();
            reader.close();

            // Get list of all files differing
            DiffFormatter diffFmt = new DiffFormatter(null);
            diffFmt.setRepository(repo);
            ObjectId head = repo.resolve(Constants.HEAD);
            ObjectId mergeHead = repo.resolve(merge_head_sha1);
            List<DiffEntry> list = diffFmt.scan(mergeHead, head);
            for (DiffEntry diffE : list) {

                String file_name = diffE.getNewPath();
                if (diffE.getChangeType() == ChangeType.DELETE)
                    file_name = diffE.getOldPath();

                if (!status_names.contains(file_name)) {
                    // Add only files not in "git status"-list to files
                    char index_status;
                    if (new File(directory, file_name).exists())
                        index_status = 'M';
                    else
                        index_status = 'D';
                    GitStatus.Entry new_e = new GitStatus.Entry(index_status, ' ', file_name);
                    statusMap.remove(file_name);
                    statusMap.put(file_name, new_e);
                }
            }
        }

        GitStatus gitStatus = new GitStatus(0, 0);
        gitStatus.commitsAhead = commitsAhead(directory, fetch);
        gitStatus.commitsBehind = commitsBehind(directory, fetch);

        for (String file : statusMap.keySet()) {
            Entry entry = statusMap.get(file);
            gitStatus.files.add(entry);
        }

        // Sort the list. We always wan't to keep the order same regardless
        // if the file came for git status or git diff
        Collections.sort(gitStatus.files, new Comparator<GitStatus.Entry>() {
            @Override
            public int compare(Entry o1, Entry o2) {
                return o1.file.compareTo(o2.file);
            }
        });

        return gitStatus;
    }

    @Override
    public CommitDesc commitAll(String directory, String message)
            throws IOException {
        if (getState(directory) == GitState.MERGE) {
            throw new GitException("commitAll is not permited when repository is in merge state. Resolve conflicts and use commit() instead.");
        }

        Git git = getGit(directory);
        CommitCommand command = git.commit()
                .setAll(true)
                .setMessage(message);
        wrapCall(command);
        Log log = log(directory, 1);
        return log.getCommits(0);
    }

    @Override
    public CommitDesc commit(String directory, String message)
            throws IOException {
        Git git = getGit(directory);
        CommitCommand command = git.commit()
                .setAll(false)
                .setMessage(message);
        wrapCall(command);
        Log log = log(directory, 1);
        return log.getCommits(0);
    }

    private ObjectId findBlobInCommit(Repository repo, ObjectId commit, String file) throws MissingObjectException, IncorrectObjectTypeException, IOException {
        RevWalk revWalk = new RevWalk(repo);
        RevCommit root = revWalk.parseCommit(commit);
        revWalk.markStart(root);
        Iterator<RevCommit> i = revWalk.iterator();
        if (i.hasNext()) {
            RevCommit revCommit = i.next();
            TreeWalk treeWalk = TreeWalk.forPath(repo, file, revCommit.getTree());
            if (treeWalk != null) {
                treeWalk.setRecursive(true);
                CanonicalTreeParser canonicalTreeParser = treeWalk.getTree(0, CanonicalTreeParser.class);

                while (!canonicalTreeParser.eof()) {
                    String p = canonicalTreeParser.getEntryPathString();
                    if (p.equals(file)) {
                        return canonicalTreeParser.getEntryObjectId();
                    }
                    canonicalTreeParser.next(1);
                }
            }
        }
        return null;
    }

    @Override
    public void resolve(String directory, String file, GitStage stage)
            throws IOException {
        if (getState(directory) != GitState.MERGE) {
            throw new GitException("resolve() is only valid operation when in MERGE state");
        }

        Git git = getGit(directory);
        Repository repo = git.getRepository();

        ObjectId head = repo.resolve(Constants.HEAD);
        ObjectId mergeHead = repo.resolve(Constants.MERGE_HEAD);

        ObjectId headBlob = findBlobInCommit(repo, head, file);
        ObjectId mergeHeadBlob = findBlobInCommit(repo, mergeHead, file);

        DirCache locked = repo.lockDirCache();
        DirCacheBuilder builder = locked.builder();

        {
            int count = locked.getEntryCount();
            for (int i = 0; i < count; ++i) {
                DirCacheEntry e = locked.getEntry(i);
                boolean skip = false;
                int s = e.getStage();
                if (e.getPathString().equals(file)) {
                    if (s == DirCacheEntry.STAGE_0 || s == DirCacheEntry.STAGE_2 || s == DirCacheEntry.STAGE_3) {
                        // Remove all entries in index with stage 0, 2 and 3
                        // We recreated stages 2 and 3 below.
                        skip = true;
                    }
                }
                if (!skip) {
                    builder.add(e);
                }
            }
        }

        if (headBlob != null) {
            DirCacheEntry ours = new DirCacheEntry(file, DirCacheEntry.STAGE_2);
            ObjectLoader oursLoader = repo.open(headBlob);
            ours.setUpdateNeeded(true);
            ours.setObjectId(headBlob);
            ours.setLength(oursLoader.getSize());
            ours.setFileMode(FileMode.REGULAR_FILE);
            builder.add(ours);
        }

        if (mergeHeadBlob != null) {
            DirCacheEntry theirs = new DirCacheEntry(file, DirCacheEntry.STAGE_3);
            ObjectLoader theirsLoader = repo.open(mergeHeadBlob);
            theirs.setUpdateNeeded(true);
            theirs.setObjectId(mergeHeadBlob);
            theirs.setLength(theirsLoader.getSize());
            theirs.setFileMode(FileMode.REGULAR_FILE);
            builder.add(theirs);
        }

        builder.finish();
        builder.commit();
        locked.unlock();

        GitStatus gitStatus = getStatus(directory, true);
        Entry entry = null;
        for (Entry e : gitStatus.files) {
            if (e.file.equals(file)) {
                entry = e;
                break;
            }
        }

        if (entry == null)
            throw new GitException(String.format("Status for '%s' not found", file));

        DirCache dirCache = repo.readDirCache();
        int count = dirCache.getEntryCount();

        DirCacheEntry foundEntry = null;

        for (int i = 0; i < count; ++i) {
            DirCacheEntry e = dirCache.getEntry(i);
            if (e.getPathString().equals(file) && e.getStage() == stage.stage) {
                foundEntry = e;
                break;
            }
        }

        if (entry.workingTreeStatus == 'D' && stage == GitStage.THEIRS) {
            // Deleted by them
            RmCommand command = git.rm().addFilepattern(file);
            wrapCall(command);
        }
        else if (entry.indexStatus == 'D' && stage == GitStage.YOURS) {
            // Deleted by us
            RmCommand command = git.rm().addFilepattern(file);
            wrapCall(command);
        }
        else {
            // Default behaviour. Checkout file from index.
            if (foundEntry != null) {
                ObjectLoader loader = repo.open(foundEntry.getObjectId());
                FileOutputStream out = new FileOutputStream(directory + "/" + file);
                loader.copyTo(out);
                out.close();
            } else {
                throw new IOException(String.format("Unable to resolve. File %s not found", file));
            }

            AddCommand command = git.add().addFilepattern(file);
            wrapCall(command);
        }
    }

    @Override
    public void push(String directory) throws IOException {
        Git git = getGit(directory);
        PushCommand command = git.push().setTransportConfigCallback(this);
        try {
            Iterable<PushResult> iter = command.call();
            for (PushResult pushResult : iter) {
                Collection<RemoteRefUpdate> updates = pushResult.getRemoteUpdates();
                for (RemoteRefUpdate u : updates) {
                    if (u.getStatus() != RemoteRefUpdate.Status.OK) {
                        throw new GitException(String.format("Push failed (%s)", u.getStatus()));
                    }
                }
            }
        } catch (JGitInternalException e) {
            throw new GitException(e);
        } catch (InvalidRemoteException e) {
            throw new GitException(e);
        }
    }

    @Override
    public void pushInitial(String directory) throws IOException {
        throw new RuntimeException("Not implemented.");
    }

    private <T> void wrapCall(GitCommand<T> command) throws IOException {
        try {
            command.call();
        } catch (Exception e) {
            throw new GitException(e);
        }
    }

    @Override
    public void add(String directory, String file) throws IOException {
        Git git = getGit(directory);
        AddCommand add = git.add().addFilepattern(file);
        wrapCall(add);
    }

    @Override
    public int commitsAhead(String directory, boolean fetch) throws IOException {
        Git git = getGit(directory);
        if (fetch) {
            wrapCall(git.fetch().setTransportConfigCallback(this));
        }

        ObjectId head = git.getRepository().resolve(Constants.HEAD);
        ObjectId originMaster = git.getRepository().resolve("origin/master");
        LogCommand command = git.log().addRange(originMaster, head);
        try {
            Iterable<RevCommit> iter = command.call();
            int count = 0;
            for (@SuppressWarnings("unused") RevCommit revCommit : iter) {
                ++count;
            }
            return count;
        } catch (Exception e) {
            throw new IOException(e);
        }
    }

    @Override
    public int commitsBehind(String directory, boolean fetch) throws IOException {
        Git git = getGit(directory);
        if (fetch) {
            wrapCall(git.fetch().setTransportConfigCallback(this));
        }

        ObjectId head = git.getRepository().resolve(Constants.HEAD);
        ObjectId originMaster = git.getRepository().resolve("origin/master");
        LogCommand command = git.log().addRange(head, originMaster);
        try {
            Iterable<RevCommit> iter = command.call();
            int count = 0;
            for (@SuppressWarnings("unused") RevCommit revCommit : iter) {
                ++count;
            }
            return count;
        } catch (Exception e) {
            throw new IOException(e);
        }
    }

    @Override
    public void rm(String directory, String file, boolean recursive,
            boolean force) throws IOException {
        Git git = getGit(directory);
        RmCommand command = git.rm().addFilepattern(file);
        wrapCall(command);
    }

    @Override
    public void mv(String directory, String source, String destination,
            boolean force) throws IOException {
        Git git = getGit(directory);

        File sourceFile = new File(directory + "/" + source);
        File destFile = new File(directory + "/" + destination);

        if (sourceFile.isDirectory() && sourceFile.list().length == 0) {
            // Git doesn't track emtpy dirs. Just move the directory
            sourceFile.delete();
            destFile.mkdir();
        } else {
            if (sourceFile.isDirectory()) {
                FileUtils.copyDirectory(sourceFile, destFile);
            } else {
                FileInputStream in = null;
                FileOutputStream out = null;

                try {
                    in = new FileInputStream(sourceFile);
                    String p = FilenameUtils.getPath(destination);
                    new File(directory, p).mkdir();
                    out = new FileOutputStream(destFile);
                    IOUtils.copy(in, out);
                } finally {
                    if (in != null)
                        in.close();
                    if (out != null) {
                        out.close();
                    }
                }
            }

            AddCommand addCommand = git.add().addFilepattern(destination);
            try {
                addCommand.call();
            } catch (NoFilepatternException e) {
                new File(directory + "/" + destination).delete();
                throw new IOException(e);
            }

            RmCommand rmCommand = git.rm().addFilepattern(source);
            try {
                rmCommand.call();
            } catch (NoFilepatternException e) {
                throw new IOException(e);
            }
        }
    }

    @Override
    public void checkout(String directory, String file, boolean force)
            throws IOException {

        // TODO:
        // This might be a bug in JGit
        // We create directory where the file shoudl reside
        String p = FilenameUtils.getPath(file);
        File f = new File(directory, p);
        if (!f.exists()) {
            f.mkdirs();
        }

        Git git = getGit(directory);
        CheckoutCommand command = git.checkout().addPath(file).setForce(force);
        try {
            command.call();
        } catch (Exception e) {
            throw new GitException(e);
        }
    }

    @Override
    public void reset(String directory, GitResetMode resetMode, String file,
            String target) throws IOException {
        Git git = getGit(directory);
        ResetType resetType = null;

        switch(resetMode) {
        case HARD:
            resetType = ResetType.HARD;
            break;
        case KEEP:
            resetType = ResetType.KEEP;
            break;
        case MERGE:
            resetType = ResetType.MERGE;
            break;
        case MIXED:
            resetType = ResetType.MIXED;
            break;
        case SOFT:
            resetType = ResetType.SOFT;
            break;
        }

        ResetCommand command = git.reset().setRef(target);
        if (file != null) {
            command = command.addPath(file);
        } else {
            command = command.setMode(resetType);
        }
        command.call();
    }

    @Override
    public void initBare(String path, String group) throws IOException {
        throw new RuntimeException("Not implemented.");
    }

    @Override
    public void rmRepo(String path) throws IOException {
        Result r = CommandUtil.execCommand(new String[] {"rm", "-rf", path});
        if (r.exitValue > 0) {
            throw new GitException(r.stdErr.toString() + "\n" + r.stdOut.toString());
        }
    }

    @Override
    public byte[] show(String directory, String file, String revision)
            throws IOException {
        Git git = getGit(directory);

        Repository repo = git.getRepository();
        ObjectId revisionId = repo.resolve(revision);
        if (revisionId == null) {
            throw new IOException(String.format("Revision '%s' not found", revision));
        }

        ObjectId blob = findBlobInCommit(repo, revisionId, file);
        if (blob != null) {
            ObjectLoader loader = repo.open(blob);
            return loader.getBytes();

        } else {
            throw new IOException(String.format("Revision '%s' for file '%s' not found", revision, file));
        }
    }

    @Override
    public Log log(String directory, int maxCount) throws IOException {
        Git git = getGit(directory);
        LogCommand command = git.log();

        try {
            Iterable<RevCommit> iterable = command.call();

            Log.Builder logBuilder = Log.newBuilder();
            int i = 0;
            for (RevCommit c : iterable) {
                if (i >= maxCount)
                    break;
                Date date = new Date(c.getCommitTime());

                logBuilder.addCommits(CommitDesc.newBuilder()
                        .setId(c.getId().name())
                        .setMessage(c.getFullMessage())
                        .setName(c.getAuthorIdent().getName())
                        .setEmail(c.getAuthorIdent().getEmailAddress())
                        .setDate(DateFormat.getInstance().format(date))
                        .build());
                ++i;
            }
            return logBuilder.build();
        } catch (Exception e) {
            throw new IOException(e);
        }
    }

    @Override
    public void configUser(String directory, String email, String name)
            throws IOException {
        Repository repo = buildRepository(directory);
        File path = repo.getFS().resolve(new File(directory, ".git"), "config");
        FileBasedConfig cfg = new FileBasedConfig(path, repo.getFS());
        try {
            cfg.load();
            cfg.setString("user", null, "name", name);
            cfg.setString("user", null, "email", email);
            cfg.save();
        } catch (ConfigInvalidException e) {
            throw new GitException(e);
        }
    }

    @Override
    public void config(String directory, String key, String value)
            throws IOException {
        Repository repo = buildRepository(directory);
        File path = repo.getFS().resolve(new File(directory, ".git"), "config");
        FileBasedConfig cfg = new FileBasedConfig(path, repo.getFS());
        try {
            cfg.load();
            cfg.setString("user", null, key, value);
            cfg.save();
        } catch (ConfigInvalidException e) {
            throw new GitException(e);
        }
    }

    @Override
    public boolean checkGitVersion() {
        throw new RuntimeException("Not implemented.");
    }

    @Override
    public void setUsername(String userName) {
        this.userName = userName;
    }

    @Override
    public void setPassword(String passWord) {
        this.passWord = passWord;
    }

    @Override
    public void setHost(String host) {
        throw new RuntimeException("Not implemented.");
    }
}
