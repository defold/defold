package com.dynamo.server.dgit;

import java.io.IOException;

public class GitException extends IOException {

    /**
     *
     */
    private static final long serialVersionUID = 5133535096852292587L;

    public GitException(String string) {
        super(string);
    }

    public GitException(Throwable e) {
        super(e);
    }

}
