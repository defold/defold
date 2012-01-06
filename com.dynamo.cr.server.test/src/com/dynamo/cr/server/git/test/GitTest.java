package com.dynamo.cr.server.git.test;

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

import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.protocol.proto.Protocol.CommitDesc;
import com.dynamo.cr.protocol.proto.Protocol.Log;
import com.dynamo.server.git.CommandUtil;
import com.dynamo.server.git.Git;
import com.dynamo.server.git.GitException;
import com.dynamo.server.git.GitResetMode;
import com.dynamo.server.git.GitStage;
import com.dynamo.server.git.GitState;
import com.dynamo.server.git.GitStatus;

public class GitTest {

    private Git git;

    void execCommand(String command) throws IOException {
        CommandUtil.Result r = CommandUtil.execCommand(new String[] {"/bin/bash", command});
        if (r.exitValue != 0) {
            System.err.println(r.stdOut);
            System.err.println(r.stdErr);
        }
        assertEquals(0, r.exitValue);
    }

    @Before
    public void setUp() throws IOException, InterruptedException {
        execCommand("scripts/setup_testgit_repo.sh");
        git = new Git();
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

        execCommand("scripts/add_source_repo_commit.sh");

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

    @Test(expected = GitException.class)
    public void pullWithDirtyState() throws IOException {
        File repo = cloneInitial();

        execCommand("scripts/add_source_repo_commit.sh");

        FileWriter fw = new FileWriter(new File(repo, "main.cpp"));
        fw.write("testing\n");
        fw.close();

        git.pull(repo.getPath());
    }

    @Test
    public void mergeConflict() throws IOException {
        File repo = cloneInitial();

        execCommand("scripts/add_source_repo_commit.sh");

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

        execCommand("scripts/add_source_repo_commit.sh");

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

        execCommand("scripts/add_source_repo_commit.sh");
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

        execCommand("scripts/delete_source_repo_commit.sh");
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

        execCommand("scripts/add_source_repo_commit.sh");
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

        execCommand("scripts/add_source_repo_commit.sh");
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

        execCommand("scripts/delete_source_repo_commit.sh");
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

        execCommand("scripts/add_source_repo_commit.sh");
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

        execCommand("scripts/add_source_repo_commit.sh");
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
        execCommand("scripts/add_source_repo_commit.sh");

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
