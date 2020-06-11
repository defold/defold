package com.defold.editor;

import com.defold.editor.fs.IResource;


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

    public CompileExceptionError(IResource resource, int lineNumber, Exception e) {
        super(e.getMessage());
        this.resource = resource;
        this.lineNumber = lineNumber;
    }

    public CompileExceptionError(String message, Throwable e) {
        super(message, e);
        this.resource = null;
        this.lineNumber = -1;
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
