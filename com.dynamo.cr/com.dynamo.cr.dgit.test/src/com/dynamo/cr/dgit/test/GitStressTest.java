package com.dynamo.cr.dgit.test;


import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Random;
import java.util.Scanner;

import org.eclipse.jgit.util.FileUtils;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;

import com.dynamo.server.dgit.CommandUtil;
import com.dynamo.server.dgit.GitException;
import com.dynamo.server.dgit.GitFactory;
import com.dynamo.server.dgit.GitFactory.Type;
import com.dynamo.server.dgit.GitStage;
import com.dynamo.server.dgit.GitState;
import com.dynamo.server.dgit.GitStatus;
import com.dynamo.server.dgit.IGit;

@RunWith(value = Parameterized.class)
public class GitStressTest {

    private final IGit git;

    public static final String MASTER = "tmp/source_repo";
    public static final String FILE_FMT = "%d.txt";
    public static final int FILE_COUNT = 10;
    public static final int LINE_COUNT = 10;

    public GitStressTest(Type type) {
        this.git = GitFactory.create(type);
    }

    @Parameters
    public static Collection<Type[]> data() {
        // No support for jgit yet
        Type[][] data = new Type[][] { /*{ Type.JGIT }, */{ Type.CGIT } };
        return Arrays.asList(data);
    }

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
        RepoUtil.setupTestRepo(this.git);
    }

    private static class Client {

        public enum Name {
            A,
            B
        }

        private final Name name;
        private final IGit git;
        private final String repo;
        private final Random rand;
        private final Name[][] data;
        private int commitCount;
        private long lastPush;

        public Client(Name name, IGit git) throws IOException {
            this.name = name;
            this.git = git;
            File f = new File("tmp", this.name.name().toLowerCase() + "_repo");
            if (f.exists()) {
                FileUtils.delete(f);
            }
            this.repo = f.getPath();
            this.git.cloneRepo(MASTER, this.repo);
            this.rand = new Random(name.hashCode());
            this.data = new Name[FILE_COUNT][];
        }

        public Name[][] getData() {
            return this.data;
        }

        public long getLastPush() {
            return this.lastPush;
        }

        public void work() throws IOException {
            synchronize();
            edit();
            saveData(this.repo, this.data, this.git);
            synchronize();
        }

        public void synchronize() throws IOException {
            GitState state = this.git.getState(this.repo);
            // commit
            if (state == GitState.DIRTY) {
                stage();
                commit();
            }
            GitStatus status = this.git.getStatus(this.repo, true);
            while (status.commitsBehind > 0) {
                pullAndResolve();
                status = this.git.getStatus(this.repo, true);
            }
            if (status.commitsAhead > 0) {
                while (true) {
                    try {
                        this.git.push(this.repo);
                        this.lastPush = System.currentTimeMillis();
                        break;
                    } catch (GitException e) {
                        // Assuming someone pushed before us
                        pullAndResolve();
                    }
                }
            }
        }

        private void pullAndResolve() throws IOException {
            boolean result = this.git.pull(this.repo);
            // merge
            if (!result) {
                resolve();
                commit();
            }
            loadData(this.repo, this.data);
        }

        private void stage() throws IOException {
            // TODO see saveData
            GitStatus status = this.git.getStatus(this.repo, true);
            for (GitStatus.Entry entry : status.files) {
                char w = entry.workingTreeStatus;
                if (w == 'D') {
                    this.git.rm(this.repo, entry.file, false, false);
                } else {
                    this.git.add(this.repo, entry.file);
                }
            }
        }

        private void resolve() throws IOException {
            GitStatus status = this.git.getStatus(this.repo, true);
            for (GitStatus.Entry entry : status.files) {
                int i = this.rand.nextInt(2);
                if (i == 0) {
                    this.git.resolve(this.repo, entry.file, GitStage.YOURS);
                } else {
                    this.git.resolve(this.repo, entry.file, GitStage.THEIRS);
                }
            }
        }

        private void commit() throws IOException {
            this.git.commit(this.repo, this.name + " commit " + this.commitCount++);
        }

        private void edit() throws IOException {
            // obtain random file set to edit
            List<Integer> indices = new ArrayList<Integer>(FILE_COUNT);
            for (int i = 0; i < FILE_COUNT; ++i) {
                indices.add(i);
            }
            Collections.shuffle(indices, this.rand);
            int count = this.rand.nextInt(FILE_COUNT-1) + 1;
            for (int i = 0; i < count; ++i) {
                int index = indices.get(i);
                // create
                if (this.data[index] == null) {
                    this.data[index] = new Name[LINE_COUNT];
                    Arrays.fill(this.data[index], this.name);
                } else {
                    int op = this.rand.nextInt(4);
                    if (op < 3) {
                        // modify
                        int j = this.rand.nextInt(LINE_COUNT);
                        this.data[index][j] = this.name;
                    } else {
                        // delete
                        this.data[index] = null;
                    }
                }
            }
        }

        private static void loadData(String dir, Name[][] data) throws IOException {
            File d = new File(dir);
            for (int i = 0; i < FILE_COUNT; ++i) {
                String fileName = String.format(FILE_FMT, i);
                File f = new File(d, fileName);
                if (f.exists()) {
                    if (data[i] == null) {
                        data[i] = new Name[LINE_COUNT];
                    }
                    String[] lines = getLines(f);
                    for (int j = 0; j < LINE_COUNT; ++j) {
                        data[i][j] = Name.valueOf(lines[j]);
                    }
                } else {
                    data[i] = null;
                }
            }
        }

        private static String[] getLines(File f) throws FileNotFoundException {
            Scanner s = new Scanner(f);
            List<String> lines = new ArrayList<String>();
            while (s.hasNextLine()) {
                lines.add(s.nextLine());
            }
            s.close();
            String[] result = new String[lines.size()];
            lines.toArray(result);
            return result;
        }

        private static void saveData(String dir, Name[][] data, IGit git) throws IOException {
            File d = new File(dir);
            for (int i = 0; i < FILE_COUNT; ++i) {
                String fileName = String.format(FILE_FMT, i);
                File f = new File(d, fileName);
                if (data[i] != null) {
                    StringBuffer buffer = new StringBuffer();
                    for (Name n : data[i]) {
                        buffer.append(n.name()).append("\n");
                    }
                    FileWriter fw = new FileWriter(f);
                    fw.write(buffer.toString().toCharArray());
                    fw.flush();
                    fw.close();
                    // Fake mtime
                    f.setLastModified(System.currentTimeMillis() + 1000);
                } else {
                    f.delete();
                }
            }
        }
    }

    @Test //(timeout=1000)
    public void testStress() throws Exception {
        Client u1 = new Client(Client.Name.A, this.git);
        Client u2 = new Client(Client.Name.B, this.git);
        final boolean sync = true;
        if (sync) {
            final int iterations = 10;
            syncTest(u1, u2, iterations);
        } else {
            final long timeout = 1000;
            asyncTest(u1, u2, timeout);
        }

        // Synchronize client who didn't do the last push
        if (u1.getLastPush() > u2.getLastPush()) {
            u2.synchronize();
        } else {
            u1.synchronize();
        }
        for (int i = 0; i < FILE_COUNT; ++i) {
            boolean dump = false;
            if (dump) {
                String filename = String.format(FILE_FMT, i);
                System.out.println("File: " + filename);
                for (int j = 0; j < LINE_COUNT; ++j) {
                    String a = "0";
                    String b = "0";
                    if (u1.getData()[i] != null) {
                        a = u1.getData()[i][j].name();
                    }
                    if (u2.getData()[i] != null) {
                        b = u2.getData()[i][j].name();
                    }
                    System.out.println(a + " " + b);
                }
            }
            assertTrue(Arrays.equals(u1.getData()[i], u2.getData()[i]));
        }
    }

    private void syncTest(Client u1, Client u2, int iterations) throws Exception {
        for (int i = 0; i < iterations; ++i) {
            u1.work();
            u2.work();
        }
    }

    private static class ClientRunner implements Runnable {
        private Client c;
        private boolean alive;

        public ClientRunner(Client c) {
            this.c = c;
        }

        public void stop() {
            this.alive = false;
        }

        @Override
        public void run() {
            this.alive = true;
            while (this.alive) {
                try {
                    this.c.work();
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
                try {
                    Thread.sleep(10);
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
            }
        }
    }

    private void asyncTest(Client c1, Client c2, long timeout) throws Exception {
        ClientRunner cr1 = new ClientRunner(c1);
        ClientRunner cr2 = new ClientRunner(c2);
        Thread t1 = new Thread(cr1);
        Thread t2 = new Thread(cr2);
        t1.start();
        t2.start();
        long start = System.currentTimeMillis();
        while (start + timeout > System.currentTimeMillis()) {
            try {
                Thread.sleep(10);
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }
        cr1.stop();
        cr2.stop();
        t1.join();
        t2.join();
    }

}
