package com.dynamo.bob;


/**
 * Compile exception
 * @author Christian Murray
 *
 */
public class CompileExceptionError extends Exception {
    private static final long serialVersionUID = -3189379067765141096L;

    private IResource resource;

    public CompileExceptionError(String message) {
        super(message);
    }

    public CompileExceptionError(String message, Throwable e) {
        super(message, e);
    }

    public CompileExceptionError(String message, Throwable e,
            IResource resource) {
        super(message, e);
        this.resource = resource;
    }

    public IResource getResource() {
        return resource;
    }

}
