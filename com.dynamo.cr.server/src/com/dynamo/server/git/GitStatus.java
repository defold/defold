package com.dynamo.server.git;

import java.util.ArrayList;
import java.util.List;

public class GitStatus {

    public static class Entry {
        public Entry(char index_status, char work_status, String file) {
            this.indexStatus = index_status;
            this.workStatus = work_status;
            this.file = file;
        }
        public char indexStatus, workStatus;
        public String file;
        public String original;
    }

    public List<Entry> files = new ArrayList<GitStatus.Entry>();
    public int commitsAhead;
    public int commitsBehind;

    public GitStatus(int commitsAhead, int commitBehind) {
        this.commitsAhead = commitsAhead;
        this.commitsBehind = commitBehind;
    }

}
