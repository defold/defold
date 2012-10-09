package com.dynamo.bob.pipeline;

public class LoaderException extends Exception {
    private static final long serialVersionUID = 8396408423837023974L;

    public LoaderException(String message) {
        super(message);
    }

    public LoaderException(String message, Throwable e) {
        super(message, e);
    }

    public LoaderException(Throwable e) {
        super(e);
    }

}
