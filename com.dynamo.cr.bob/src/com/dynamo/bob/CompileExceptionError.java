package com.dynamo.bob;


/**
 * Compile exception
 * @author Christian Murray
 *
 */
public class CompileExceptionError extends Exception {
    private static final long serialVersionUID = -3189379067765141096L;

    private int returnCode;

    private IResource resource;

    public CompileExceptionError(String message, int returnCode) {
        super(message);
        this.returnCode = returnCode;
    }

    public CompileExceptionError(String message) {
        super(message);
        this.returnCode = 5;
    }

    public CompileExceptionError(String message, Throwable e, int returnCode) {
        super(message, e);
        this.returnCode = returnCode;
    }

    public CompileExceptionError(String message, Throwable e,
            IResource resource) {
        super(message, e);
        this.resource = resource;
    }

    public int getReturnCode() {
        return returnCode;
    }

    public IResource getResource() {
        return resource;
    }

}
