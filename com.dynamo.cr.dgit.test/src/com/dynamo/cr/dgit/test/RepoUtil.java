package com.dynamo.cr.dgit.test;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

import org.apache.commons.io.FileUtils;

import com.dynamo.server.dgit.IGit;

public class RepoUtil {

    public static final String MASTER = "tmp/source_repo";
    public static final String TMP_CLONE = "tmp/tmp_source_repo";

    public static void clearRepos() throws IOException {

        File tmp = new File("tmp");
        if (tmp.exists()) {
            String[] files = tmp.list();
            for (String f : files) {
                File file = new File(tmp, f);
                if (f.contains("repo") && file.isDirectory()) {
                    try {
                        FileUtils.deleteDirectory(file);
                    } catch (IOException e) {
                        // Fix for unknown lock-delay on windows (not anti-virus)
                        try {
                            Thread.sleep(1000);
                        } catch (InterruptedException e2) {

                        }
                        FileUtils.deleteDirectory(file);
                    }
                }
            }
        }
    }

    public static void setFile(String dir, String file, String content) throws IOException {
        File f = new File(dir, file);
        FileWriter fw = new FileWriter(f);
        fw.write(content);
        fw.close();
    }

    public static void setupTestRepo(IGit git) throws IOException {
        RepoUtil.clearRepos();
        String master = MASTER;
        org.apache.commons.io.FileUtils.forceMkdir(new File(master));
        git.initBare(master, null);
        String tmpRepo = TMP_CLONE;
        git.cloneRepo(master, tmpRepo);
        setFile(tmpRepo, "main.cpp", "#include <stdio.h>\n\n"
                + "int main()\n"
                + "{\n"
                + "    printf(\"Hello world\");\n"
                + "}\n");
        setFile(tmpRepo, "rename.cpp", "// temp\n");
        git.add(tmpRepo, "main.cpp");
        git.add(tmpRepo, "rename.cpp");
        git.commit(tmpRepo, "Initial repo");
        git.pushInitial(tmpRepo);
    }

    public static void addSourceCommit(IGit git) throws IOException {
        File f = new File(TMP_CLONE, "main.cpp");
        FileWriter fw = new FileWriter(f, true);
        fw.write("#include <stdint.h>");
        fw.close();
        git.commitAll(TMP_CLONE, "Added stdint.h");
        git.push(TMP_CLONE);
    }

    public static void deleteSourceCommit(IGit git) throws IOException {
        File f = new File(TMP_CLONE, "main.cpp");
        f.delete();
        git.rm(TMP_CLONE, "main.cpp", false, false);
        git.commitAll(TMP_CLONE, "Deleted main.cpp");
        git.push(TMP_CLONE);
    }

    public static void setupEmptyTestRepo(IGit git) throws IOException {
        RepoUtil.clearRepos();
        String master = MASTER;
        org.apache.commons.io.FileUtils.forceMkdir(new File(master));
        git.initBare(master, null);
        String tmpRepo = TMP_CLONE;
        git.cloneRepo(master, tmpRepo);
        setFile(tmpRepo, "test", "test\n");
        git.add(tmpRepo, "test");
        git.commit(tmpRepo, "Initial repo");
        git.pushInitial(tmpRepo);
    }

}
