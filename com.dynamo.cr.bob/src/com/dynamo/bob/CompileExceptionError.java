package com.dynamo.bob;


/**
 * Compile exception
 * @author Christian Murray
 *
 */
public class CompileExceptionError extends Exception {
    private static final long serialVersionUID = -3189379067765141096L;

    private IResource resource;

    public CompileExceptionError(IResource resource, String message) {
        super(message);
        this.resource = resource;
    }

    public CompileExceptionError(IResource resource, String message, Throwable e) {
        super(message, e);
        this.resource = resource;
    }

    public IResource getResource() {
        return resource;
    }

}
