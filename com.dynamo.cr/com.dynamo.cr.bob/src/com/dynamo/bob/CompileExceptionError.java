package com.dynamo.bob;

import com.dynamo.bob.fs.IResource;


/**
 * Compile exception
 * @author Christian Murray
 *
 */
public class CompileExceptionError extends Exception {
    private static final long serialVersionUID = -3189379067765141096L;

    private IResource resource;
    private int lineNumber;

    public CompileExceptionError(IResource resource, int lineNumber, String message) {
        super(message);
        this.resource = resource;
        this.lineNumber = lineNumber;
    }

    public CompileExceptionError(IResource resource, int lineNumber, String message, Throwable e) {
        super(message, e);
        this.resource = resource;
        this.lineNumber = lineNumber;
    }

    public IResource getResource() {
        return resource;
    }

    public int getLineNumber() {
        return lineNumber;
    }
}
