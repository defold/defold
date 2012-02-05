package com.dynamo.server.dgit;

public enum GitStage {
    BASE (1),
    YOURS(2),
    THEIRS(3),
    ;

    public int stage;

    GitStage(int stage_) {
        this.stage = stage_;
    }

}
