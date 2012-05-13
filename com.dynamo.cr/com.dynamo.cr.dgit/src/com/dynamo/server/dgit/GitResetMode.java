package com.dynamo.server.dgit;

public enum GitResetMode {
    SOFT(1),
    MIXED(2),
    HARD(3),
    MERGE(4),
    KEEP(5),
    ;

    public int resetMode;

    GitResetMode(int resetMode) {
        this.resetMode = resetMode;
    }

}
