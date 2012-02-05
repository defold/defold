package com.dynamo.server.dgit;

public class GitFactory {
    public static enum Type {
        CGIT,
        JGIT
    }

    public static IGit create(Type type) {
        if (type == Type.CGIT)
            return new CGit();
        else
            return new JGit();
    }
}
