package com.dynamo.server.dgit;

import java.util.ArrayList;
import java.util.List;

public class GitStatus {

    public static class Entry {
        public Entry(char indexStatus, char workingTreeStatus, String file) {
            this.indexStatus = indexStatus;
            this.workingTreeStatus = workingTreeStatus;
            this.file = file;
        }
        public char indexStatus, workingTreeStatus;
        public String file;
        public String original;

        @Override
        public String toString() {
            return String.format("[%c%c] %s -> %s", indexStatus, workingTreeStatus, original, file);
        }
    }

    public List<Entry> files = new ArrayList<GitStatus.Entry>();
    public int commitsAhead;
    public int commitsBehind;

    public GitStatus(int commitsAhead, int commitBehind) {
        this.commitsAhead = commitsAhead;
        this.commitsBehind = commitBehind;
    }

}
