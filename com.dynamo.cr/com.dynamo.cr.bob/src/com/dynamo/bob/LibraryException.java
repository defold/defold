package com.dynamo.bob;

@SuppressWarnings("serial")
public class LibraryException extends Exception {
    public LibraryException(String message, Throwable throwable) {
        super(message, throwable);
    }
}
