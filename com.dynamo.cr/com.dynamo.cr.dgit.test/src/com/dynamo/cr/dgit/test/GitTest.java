package com.dynamo.cr.dgit.test;


import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Arrays;
import java.util.Collection;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;

import com.dynamo.cr.protocol.proto.Protocol.CommitDesc;
import com.dynamo.cr.protocol.proto.Protocol.Log;
import com.dynamo.server.dgit.GitException;
import com.dynamo.server.dgit.GitFactory;
import com.dynamo.server.dgit.GitFactory.Type;
import com.dynamo.server.dgit.GitResetMode;
import com.dynamo.server.dgit.GitStage;
import com.dynamo.server.dgit.GitState;
import com.dynamo.server.dgit.GitStatus;
import com.dynamo.server.dgit.GitStatus.Entry;
import com.dynamo.server.dgit.IGit;

@RunWith(value = Parameterized.class)
public class GitTest {

    private IGit git;
    private Type type;

    public GitTest(Type type) {
        this.type = type;
    }

    @Parameters
    public static Collection<Type[]> data() {
        // No support for jgit yet
        Type[][] data = new Type[][] { /*{ Type.JGIT },*/ { Type.CGIT } };
        return Arrays.asList(data);
    }

    @Before
    public void setUp() throws IOException, InterruptedException {
        git = GitFactory.create(type);
        RepoUtil.setupTestRepo(git);
    }

    String readEntireFile(File file) throws IOException {
        BufferedReader br = new BufferedReader(new FileReader(file));
        final char [] chars = new char[(int) file.length()];
        br.read(chars);
        br.close();
        return new String(chars);
    }

    File cloneInitial() throws IOException {
        File repo = File.createTempFile("clonerepo", "_repo", new File("tmp"));
        repo.delete();
        git.cloneRepo("tmp/source_repo", repo.getPath());
        return repo;
    }

    @Test
    public void cloneRepo() throws IOException {
        File repo = cloneInitial();
        assertTrue(new File(repo, ".git").exists());
        assertTrue(new File(repo, ".git").isDirectory());
        assertTrue(new File(repo, "main.cpp").exists());
    }

    @Test
    public void pull() throws IOException {
        File repo = cloneInitial();

        git.pull(repo.getPath());
        assertEquals(-1, readEntireFile(new File(repo, "main.cpp")).indexOf("stdint"));

        RepoUtil.addSourceCommit(git);

        assertEquals(-1, readEntireFile(new File(repo, "main.cpp")).indexOf("stdint"));
        git.pull(repo.getPath());
        assertTrue(readEntireFile(new File(repo, "main.cpp")).indexOf("stdint") != -1);
    }

    @Test
    public void cleanState() throws IOException {
        File repo = cloneInitial();
        GitState state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);
    }

    @Test
    public void dirtyState() throws IOException {
        File repo = cloneInitial();
        GitState state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();
        state = git.getState(repo.getPath());
        assertEquals(GitState.DIRTY, state);
    }

    @Test
    public void checkout() throws IOException {
        File repo = cloneInitial();
        GitState state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();
        state = git.getState(repo.getPath());
        assertEquals(GitState.DIRTY, state);

        git.checkout(repo.getPath(), "main.cpp", false);

        state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);
    }

    @Test
    public void status() throws IOException {
        File repo = cloneInitial();

        GitStatus status = git.getStatus(repo.getPath());
        assertEquals(0, status.files.size());

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();

        git.mv(repo.getPath(), "rename.cpp", "rename2.cpp", true);

        status = git.getStatus(repo.getPath());
        assertEquals(2, status.files.size());

        assertEquals("main.cpp", status.files.get(0).file);
        assertEquals(' ', status.files.get(0).indexStatus);
        assertEquals('M', status.files.get(0).workingTreeStatus);
        assertEquals("rename2.cpp", status.files.get(1).file);
        assertEquals('R', status.files.get(1).indexStatus);
        assertEquals(' ', status.files.get(1).workingTreeStatus);
    }

    @Test
    public void newFile() throws IOException {
        File repo = cloneInitial();

        GitStatus status = git.getStatus(repo.getPath());
        assertEquals(0, status.files.size());

        FileWriter fw = new FileWriter(new File(repo, "new_file.txt"));
        fw.write("testing\n");
        fw.close();

        status = git.getStatus(repo.getPath());
        assertEquals(1, status.files.size());

        Entry e = status.files.get(0);
        assertEquals("new_file.txt", e.file);
        assertEquals('?', e.indexStatus);
        assertEquals('?', e.workingTreeStatus);
    }

    @Test
    public void renameEmpty() throws IOException {
        File repo = cloneInitial();

        GitStatus status = git.getStatus(repo.getPath());
        assertEquals(0, status.files.size());

        File newDir = new File(repo, "new_dir");
        File newDirPrim = new File(repo, "new_dir_prim");
        assertFalse(newDir.exists());
        assertFalse(newDirPrim.exists());

        newDir.mkdir();
        assertTrue(newDir.exists());
        assertFalse(newDirPrim.exists());

        git.mv(repo.getPath(), "new_dir", "new_dir_prim", true);
        assertFalse(newDir.exists());
        assertTrue(newDirPrim.exists());

        status = git.getStatus(repo.getPath());
        assertEquals(0, status.files.size());
    }

    @Test
    public void changeNewAndStagedFile() throws IOException {
        File repo = cloneInitial();

        GitStatus status = git.getStatus(repo.getPath());
        assertEquals(0, status.files.size());

        FileWriter fw = new FileWriter(new File(repo, "new_file.txt"));
        fw.write("testing\n");
        fw.close();

        // Stage
        git.add(repo.getPath(), "new_file.txt");

        // Change again
        fw = new FileWriter(new File(repo, "new_file.txt"), true);
        fw.write("some appended content\n");
        fw.close();

        status = git.getStatus(repo.getPath());
        assertEquals(1, status.files.size());

        Entry e = status.files.get(0);
        assertEquals("new_file.txt", e.file);
        assertEquals('A', e.indexStatus);
        assertEquals('M', e.workingTreeStatus);
    }

    @Test
    public void renameRevertDirectoryDirectory1() throws IOException {
        File repo = cloneInitial();

        GitStatus status = git.getStatus(repo.getPath());
        assertEquals(0, status.files.size());

        new File(repo, "dir").mkdir();

        FileWriter fw = new FileWriter(new File(repo, "dir/new_file.txt"));
        fw.write("testing\n");
        fw.close();

        // Commit
        git.add(repo.getPath(), "dir/new_file.txt");
        git.commitAll(repo.getPath(), "commit1");

        // Move
        git.mv(repo.getPath(), "dir", "dir2", false);
        git.commitAll(repo.getPath(), "commit2");

        // NOTE: We move the file here and not th dir. See test below for the other version
        git.mv(repo.getPath(), "dir2/new_file.txt", "dir/new_file.txt", false);
        git.checkout(repo.getPath(), "dir/new_file.txt", false);
        git.commitAll(repo.getPath(), "commit2");

        status = git.getStatus(repo.getPath());
        assertEquals(0, status.files.size());

        String content = readEntireFile(new File(repo, "dir/new_file.txt"));
        assertEquals("testing\n", content);
    }

    @Test
    public void renameRevertDirectoryDirectory2() throws IOException {
        File repo = cloneInitial();

        GitStatus status = git.getStatus(repo.getPath());
        assertEquals(0, status.files.size());

        new File(repo, "dir").mkdir();

        FileWriter fw = new FileWriter(new File(repo, "dir/new_file.txt"));
        fw.write("testing\n");
        fw.close();

        // Commit
        git.add(repo.getPath(), "dir/new_file.txt");
        git.commitAll(repo.getPath(), "commit1");

        // Move
        git.mv(repo.getPath(), "dir", "dir2", false);
        git.commitAll(repo.getPath(), "commit2");

        // Move back
        git.mv(repo.getPath(), "dir2", "dir", false);
        git.commitAll(repo.getPath(), "commit2");

        status = git.getStatus(repo.getPath());
        assertEquals(0, status.files.size());

        String content = readEntireFile(new File(repo, "dir/new_file.txt"));
        assertEquals("testing\n", content);
    }

    @Test
    public void unstageDeleted() throws IOException {
        File repo = cloneInitial();

        GitStatus status = git.getStatus(repo.getPath());
        assertEquals(0, status.files.size());

        new File(repo, "dir").mkdir();

        FileWriter fw = new FileWriter(new File(repo, "dir/new_file.txt"));
        fw.write("testing\n");
        fw.close();

        // Commit
        git.add(repo.getPath(), "dir/new_file.txt");
        git.commitAll(repo.getPath(), "commit1");

        // Remove
        git.rm(repo.getPath(), "dir/new_file.txt", false, false);
        new File(repo.getPath(), "dir").delete();

        status = git.getStatus(repo.getPath());
        assertEquals(1, status.files.size());
        Entry e = status.files.get(0);
        assertEquals("dir/new_file.txt", e.file);
        assertEquals('D', e.indexStatus);
        assertEquals(' ', e.workingTreeStatus);

        git.reset(repo.getPath(), GitResetMode.MIXED, "dir/new_file.txt", "HEAD");
        git.checkout(repo.getPath(), "dir/new_file.txt", false);

        status = git.getStatus(repo.getPath());
        assertEquals(0, status.files.size());

        String content = readEntireFile(new File(repo, "dir/new_file.txt"));
        assertEquals("testing\n", content);
    }

    @Test
    public void commit() throws IOException {
        File repo = cloneInitial();
        GitState state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);

        Log log = git.log(repo.getPath(), 5);
        assertEquals(1, log.getCommitsCount());

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();
        state = git.getState(repo.getPath());
        assertEquals(GitState.DIRTY, state);

        CommitDesc commit = git.commitAll(repo.getPath(), "test commit");
        state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);

        log = git.log(repo.getPath(), 5);
        assertEquals(2, log.getCommitsCount());
        assertEquals(commit.getId(), log.getCommits(0).getId());
    }

    @Test
    public void commitAllQuotedMessage() throws IOException {
        File repo = cloneInitial();
        GitState state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();
        state = git.getState(repo.getPath());
        assertEquals(GitState.DIRTY, state);

        String message = "test commit \"with quotes\"";
        CommitDesc commit = git.commitAll(repo.getPath(), message);
        state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);
        assertEquals(commit.getMessage(), message);
    }

    @Test
    public void commitQuotedMessage() throws IOException {
        File repo = cloneInitial();
        GitState state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();
        state = git.getState(repo.getPath());
        assertEquals(GitState.DIRTY, state);

        String message = "test commit \"with quotes\"";
        git.add(repo.getPath(), "main.cpp");
        CommitDesc commit = git.commit(repo.getPath(), message);
        state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);
        assertEquals(commit.getMessage(), message);
    }

    @Test(expected = GitException.class)
    public void pullWithDirtyState() throws IOException {
        File repo = cloneInitial();

        RepoUtil.addSourceCommit(git);

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();

        git.pull(repo.getPath());
    }

    @Test
    public void mergeConflict() throws IOException {
        File repo = cloneInitial();

        RepoUtil.addSourceCommit(git);

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();
        git.commitAll(repo.getPath(), "test commit");

        boolean r = git.pull(repo.getPath());
        assertEquals(false, r);

        GitState state = git.getState(repo.getPath());
        assertEquals(GitState.MERGE, state);
    }

    @Test
    public void localCommitThenPull() throws IOException {
        File repo = cloneInitial();

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();
        git.commitAll(repo.getPath(), "test commit");

        git.pull(repo.getPath());
    }

    @Test(expected = GitException.class)
    public void unresolvedMerge() throws IOException {
        File repo = cloneInitial();

        RepoUtil.addSourceCommit(git);

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();
        git.commitAll(repo.getPath(), "test commit");

        boolean r = git.pull(repo.getPath());
        assertEquals(false, r);

        GitState state = git.getState(repo.getPath());
        assertEquals(GitState.MERGE, state);

        git.commit(repo.getPath(), "test commit 2");
    }

    @Test(expected = GitException.class)
    public void invalidResolve1() throws IOException {
        File repo = cloneInitial();
        git.resolve(repo.getPath(), "main.cpp", GitStage.YOURS);
    }

    @Test
    public void resolveYours() throws IOException {
        File repo = cloneInitial();

        assertEquals(0, git.commitsAhead(repo.getPath()));
        assertEquals(0, git.commitsBehind(repo.getPath()));

        RepoUtil.addSourceCommit(git);
        assertEquals(-1, readEntireFile(new File(repo, "main.cpp")).indexOf("testing"));

        assertEquals(0, git.commitsAhead(repo.getPath()));
        assertEquals(1, git.commitsBehind(repo.getPath()));

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();
        git.commitAll(repo.getPath(), "test commit");

        assertEquals(1, git.commitsAhead(repo.getPath()));
        assertEquals(1, git.commitsBehind(repo.getPath()));
        GitStatus status = git.getStatus(repo.getPath());
        // Check this again. Test added due to bug.
        assertEquals(1, status.commitsAhead);
        assertEquals(1, status.commitsBehind);

        boolean r = git.pull(repo.getPath());
        assertEquals(false, r);

        GitState state = git.getState(repo.getPath());
        assertEquals(GitState.MERGE, state);
        status = git.getStatus(repo.getPath());
        assertEquals('U', status.files.get(0).indexStatus);
        assertEquals('U', status.files.get(0).workingTreeStatus);

        git.resolve(repo.getPath(), "main.cpp", GitStage.YOURS);
        status = git.getStatus(repo.getPath());
        assertEquals('M', status.files.get(0).indexStatus);
        assertEquals(' ', status.files.get(0).workingTreeStatus);

        git.commit(repo.getPath(), "test commit 2");
        git.push(repo.getPath());

        assertEquals(0, git.commitsAhead(repo.getPath()));
        assertEquals(0, git.commitsBehind(repo.getPath()));

        state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);

        assertTrue(readEntireFile(new File(repo, "main.cpp")).indexOf("testing") != -1);
    }

    @Test
    public void resolveYoursTheirsDeleted() throws IOException {
        File repo = cloneInitial();

        assertEquals(0, git.commitsAhead(repo.getPath()));
        assertEquals(0, git.commitsBehind(repo.getPath()));

        RepoUtil.deleteSourceCommit(git);
        assertEquals(-1, readEntireFile(new File(repo, "main.cpp")).indexOf("testing"));

        assertEquals(0, git.commitsAhead(repo.getPath()));
        assertEquals(1, git.commitsBehind(repo.getPath()));

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();
        git.commitAll(repo.getPath(), "test commit");

        assertEquals(1, git.commitsAhead(repo.getPath()));
        assertEquals(1, git.commitsBehind(repo.getPath()));

        boolean r = git.pull(repo.getPath());
        assertEquals(false, r);

        GitState state = git.getState(repo.getPath());
        assertEquals(GitState.MERGE, state);
        GitStatus status = git.getStatus(repo.getPath());
        assertEquals('U', status.files.get(0).indexStatus);
        assertEquals('D', status.files.get(0).workingTreeStatus);

        git.resolve(repo.getPath(), "main.cpp", GitStage.YOURS);
        status = git.getStatus(repo.getPath());
        assertEquals('M', status.files.get(0).indexStatus);
        assertEquals(' ', status.files.get(0).workingTreeStatus);
        git.commit(repo.getPath(), "test commit 2");
        git.push(repo.getPath());

        assertEquals(0, git.commitsAhead(repo.getPath()));
        assertEquals(0, git.commitsBehind(repo.getPath()));

        state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);
        assertTrue(readEntireFile(new File(repo, "main.cpp")).indexOf("testing") != -1);
    }

    @Test
    public void resolveYoursYoursDeleted() throws IOException {
        File repo = cloneInitial();

        assertEquals(0, git.commitsAhead(repo.getPath()));
        assertEquals(0, git.commitsBehind(repo.getPath()));

        RepoUtil.addSourceCommit(git);
        assertEquals(-1, readEntireFile(new File(repo, "main.cpp")).indexOf("testing"));

        assertEquals(0, git.commitsAhead(repo.getPath()));
        assertEquals(1, git.commitsBehind(repo.getPath()));

        git.rm(repo.getPath(), "main.cpp", false, false);
        git.commitAll(repo.getPath(), "test commit");

        assertEquals(1, git.commitsAhead(repo.getPath()));
        assertEquals(1, git.commitsBehind(repo.getPath()));

        boolean r = git.pull(repo.getPath());
        assertEquals(false, r);

        GitState state = git.getState(repo.getPath());
        assertEquals(GitState.MERGE, state);
        GitStatus status = git.getStatus(repo.getPath());
        assertEquals('D', status.files.get(0).indexStatus);
        assertEquals('U', status.files.get(0).workingTreeStatus);

        git.resolve(repo.getPath(), "main.cpp", GitStage.YOURS);
        status = git.getStatus(repo.getPath());
        assertEquals('D', status.files.get(0).indexStatus);
        assertEquals(' ', status.files.get(0).workingTreeStatus);
        git.commit(repo.getPath(), "test commit 2");
        git.push(repo.getPath());

        assertEquals(0, git.commitsAhead(repo.getPath()));
        assertEquals(0, git.commitsBehind(repo.getPath()));

        state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);

        assertTrue(!new File(repo, "main.cpp").exists());
    }

    @Test
    public void resolveTheirs() throws IOException {
        File repo = cloneInitial();

        RepoUtil.addSourceCommit(git);
        assertEquals(-1, readEntireFile(new File(repo, "main.cpp")).indexOf("testing"));

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();
        git.commitAll(repo.getPath(), "test commit");

        boolean r = git.pull(repo.getPath());
        assertEquals(false, r);

        assertTrue(readEntireFile(new File(repo, "main.cpp")).indexOf("<<<<<<< HEAD") != -1);

        GitState state = git.getState(repo.getPath());
        assertEquals(GitState.MERGE, state);
        GitStatus status = git.getStatus(repo.getPath());
        assertEquals('U', status.files.get(0).indexStatus);
        assertEquals('U', status.files.get(0).workingTreeStatus);

        git.resolve(repo.getPath(), "main.cpp", GitStage.THEIRS);
        status = git.getStatus(repo.getPath());
        assertEquals('M', status.files.get(0).indexStatus);
        assertEquals(' ', status.files.get(0).workingTreeStatus);

        git.commit(repo.getPath(), "test commit 2");
        status = git.getStatus(repo.getPath());
        assertEquals(0, status.files.size());

        git.push(repo.getPath());

        state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);

        assertEquals(-1, readEntireFile(new File(repo, "main.cpp")).indexOf("<<<<<<< HEAD"));
        assertEquals(-1, readEntireFile(new File(repo, "main.cpp")).indexOf("testing"));
        assertTrue(readEntireFile(new File(repo, "main.cpp")).indexOf("stdint") != -1);
    }

    @Test
    public void resolveTheirsTheirsDeleted() throws IOException {
        File repo = cloneInitial();

        assertEquals(0, git.commitsAhead(repo.getPath()));
        assertEquals(0, git.commitsBehind(repo.getPath()));

        RepoUtil.deleteSourceCommit(git);
        assertEquals(-1, readEntireFile(new File(repo, "main.cpp")).indexOf("testing"));

        assertEquals(0, git.commitsAhead(repo.getPath()));
        assertEquals(1, git.commitsBehind(repo.getPath()));

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();
        git.commitAll(repo.getPath(), "test commit");

        assertEquals(1, git.commitsAhead(repo.getPath()));
        assertEquals(1, git.commitsBehind(repo.getPath()));

        boolean r = git.pull(repo.getPath());
        assertEquals(false, r);

        GitState state = git.getState(repo.getPath());
        assertEquals(GitState.MERGE, state);
        GitStatus status = git.getStatus(repo.getPath());
        assertEquals('U', status.files.get(0).indexStatus);
        assertEquals('D', status.files.get(0).workingTreeStatus);

        git.resolve(repo.getPath(), "main.cpp", GitStage.THEIRS);
        status = git.getStatus(repo.getPath());
        assertEquals('D', status.files.get(0).indexStatus);
        assertEquals(' ', status.files.get(0).workingTreeStatus);
        git.commit(repo.getPath(), "test commit 2");
        git.push(repo.getPath());

        assertEquals(0, git.commitsAhead(repo.getPath()));
        assertEquals(0, git.commitsBehind(repo.getPath()));

        state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);

        assertTrue(!new File(repo, "main.cpp").exists());
    }

    @Test
    public void resolveTheirsYoursDeleted() throws IOException {
        File repo = cloneInitial();

        assertEquals(0, git.commitsAhead(repo.getPath()));
        assertEquals(0, git.commitsBehind(repo.getPath()));

        RepoUtil.addSourceCommit(git);
        assertEquals(-1, readEntireFile(new File(repo, "main.cpp")).indexOf("testing"));

        assertEquals(0, git.commitsAhead(repo.getPath()));
        assertEquals(1, git.commitsBehind(repo.getPath()));

        git.rm(repo.getPath(), "main.cpp", false, false);
        git.commitAll(repo.getPath(), "test commit");

        assertEquals(1, git.commitsAhead(repo.getPath()));
        assertEquals(1, git.commitsBehind(repo.getPath()));

        boolean r = git.pull(repo.getPath());
        assertEquals(false, r);

        GitState state = git.getState(repo.getPath());
        assertEquals(GitState.MERGE, state);
        GitStatus status = git.getStatus(repo.getPath());
        assertEquals('D', status.files.get(0).indexStatus);
        assertEquals('U', status.files.get(0).workingTreeStatus);

        git.resolve(repo.getPath(), "main.cpp", GitStage.THEIRS);
        status = git.getStatus(repo.getPath());
        assertEquals('A', status.files.get(0).indexStatus);
        assertEquals(' ', status.files.get(0).workingTreeStatus);
        git.commit(repo.getPath(), "test commit 2");
        git.push(repo.getPath());

        assertEquals(0, git.commitsAhead(repo.getPath()));
        assertEquals(0, git.commitsBehind(repo.getPath()));

        state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);

        assertEquals(-1, readEntireFile(new File(repo, "main.cpp")).indexOf("<<<<<<< HEAD"));
        assertEquals(-1, readEntireFile(new File(repo, "main.cpp")).indexOf("testing"));
        assertTrue(readEntireFile(new File(repo, "main.cpp")).indexOf("stdint") != -1);
    }

    @Test
    public void resolveYoursTheirs() throws IOException {
        // Test that we can checkout again... First YOURS then THEIRS
        File repo = cloneInitial();

        RepoUtil.addSourceCommit(git);
        assertEquals(-1, readEntireFile(new File(repo, "main.cpp")).indexOf("testing"));

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();
        git.commitAll(repo.getPath(), "test commit");

        boolean r = git.pull(repo.getPath());
        assertEquals(false, r);

        assertTrue(readEntireFile(new File(repo, "main.cpp")).indexOf("<<<<<<< HEAD") != -1);

        GitState state = git.getState(repo.getPath());
        assertEquals(GitState.MERGE, state);
        GitStatus status = git.getStatus(repo.getPath());
        assertEquals('U', status.files.get(0).indexStatus);
        assertEquals('U', status.files.get(0).workingTreeStatus);

        git.resolve(repo.getPath(), "main.cpp", GitStage.YOURS);
        status = git.getStatus(repo.getPath());
        assertEquals('M', status.files.get(0).indexStatus);
        assertEquals(' ', status.files.get(0).workingTreeStatus);
        git.resolve(repo.getPath(), "main.cpp", GitStage.THEIRS);
        status = git.getStatus(repo.getPath());
        assertEquals('M', status.files.get(0).indexStatus);
        assertEquals(' ', status.files.get(0).workingTreeStatus);
        git.commit(repo.getPath(), "test commit 2");
        git.push(repo.getPath());

        state = git.getState(repo.getPath());
        assertEquals(GitState.CLEAN, state);

        assertEquals(-1, readEntireFile(new File(repo, "main.cpp")).indexOf("<<<<<<< HEAD"));
        assertEquals(-1, readEntireFile(new File(repo, "main.cpp")).indexOf("testing"));
        assertTrue(readEntireFile(new File(repo, "main.cpp")).indexOf("stdint") != -1);
    }

    @Test
    public void push() throws IOException {
        File repo = cloneInitial();

        assertEquals(-1, readEntireFile(new File("tmp/tmp_source_repo", "main.cpp")).indexOf("testing"));

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();

        fw = new FileWriter(new File(repo, "new_file.cpp"));
        fw.write("new file\n");
        fw.close();
        git.add(repo.getPath(), "new_file.cpp");

        git.commitAll(repo.getPath(), "test commit");
        git.push(repo.getPath());
    }

    @Test(expected = GitException.class)
    public void pushFail() throws IOException {
        File repo = cloneInitial();
        RepoUtil.addSourceCommit(git);

        assertEquals(-1, readEntireFile(new File("tmp/tmp_source_repo", "main.cpp")).indexOf("testing"));

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();

        git.commitAll(repo.getPath(), "test commit");
        git.push(repo.getPath());
    }

    @Test
    public void reset() throws IOException {
        File repo = cloneInitial();

        GitStatus initStatus = git.getStatus(repo.getPath());
        assertEquals(0, initStatus.files.size());

        String file = "main.cpp";
        assertEquals(-1, readEntireFile(new File("tmp/tmp_source_repo", file)).indexOf("testing"));

        FileWriter fw = new FileWriter(new File(repo, file));
        fw.write("testing\n");
        fw.close();

        git.add(repo.getPath(), file);

        GitStatus preStatus = git.getStatus(repo.getPath());
        assertEquals(1, preStatus.files.size());
        assertEquals(file, preStatus.files.get(0).file);
        assertEquals('M', preStatus.files.get(0).indexStatus);

        Log preLog = git.log(repo.getPath(), 5);
        assertEquals(1, preLog.getCommitsCount());
        String preId = preLog.getCommits(0).getId();

        git.commit(repo.getPath(), "test commit");

        Log postLog = git.log(repo.getPath(), 5);
        assertEquals(2, postLog.getCommitsCount());

        GitStatus postStatus = git.getStatus(repo.getPath());
        assertEquals(0, postStatus.files.size());

        git.reset(repo.getPath(), GitResetMode.SOFT, null, preId);

        preStatus = git.getStatus(repo.getPath());
        assertEquals(1, preStatus.files.size());
        assertEquals(file, preStatus.files.get(0).file);
        assertEquals('M', preStatus.files.get(0).indexStatus);

        preLog = git.log(repo.getPath(), 5);
        assertEquals(1, preLog.getCommitsCount());

        git.commit(repo.getPath(), "test commit");

        postLog = git.log(repo.getPath(), 5);
        assertEquals(2, postLog.getCommitsCount());

        postStatus = git.getStatus(repo.getPath());
        assertEquals(0, postStatus.files.size());

        git.reset(repo.getPath(), GitResetMode.HARD, null, preId);

        initStatus = git.getStatus(repo.getPath());
        assertEquals(0, initStatus.files.size());

        preLog = git.log(repo.getPath(), 5);
        assertEquals(1, preLog.getCommitsCount());
    }

    @Test
    public void rmRepo() throws IOException {
        File repo = new File("tmp/source_repo");
        assertTrue(repo.isDirectory());
        git.rmRepo(repo.getAbsolutePath());
        assertFalse(repo.isDirectory());
    }

    @Test
    public void configUser() throws IOException {
        final String name = "test_name";
        final String email = "test_email";

        File repo = cloneInitial();

        assertEquals(-1, readEntireFile(new File("tmp/tmp_source_repo", "main.cpp")).indexOf("testing"));

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();

        fw = new FileWriter(new File(repo, "new_file.cpp"));
        fw.write("new file\n");
        fw.close();

        git.add(repo.getPath(), "new_file.cpp");
        git.commitAll(repo.getPath(), "test commit");
        git.push(repo.getPath());

        Log log = git.log(repo.getPath(), 1);
        assertEquals(1, log.getCommitsCount());
        CommitDesc commit = log.getCommits(0);
        assertThat(commit.getName(), not(name));
        assertThat(commit.getEmail(), not(email));

        git.configUser(repo.getPath(), email, name);

        fw = new FileWriter(new File(repo, "new_file2.cpp"));
        fw.write("new file2\n");
        fw.close();

        git.add(repo.getPath(), "new_file2.cpp");
        git.commitAll(repo.getPath(), "test commit");
        git.push(repo.getPath());

        log = git.log(repo.getPath(), 1);
        assertEquals(1, log.getCommitsCount());
        commit = log.getCommits(0);
        assertThat(commit.getName(), is(name));
        assertThat(commit.getEmail(), is(email));
    }

}
