package com.dynamo.bob;

/**
 * Compile exception
 * @author Christian Murray
 *
 */
public class CompileExceptionError extends Exception {
    private static final long serialVersionUID = -3189379067765141096L;

    private int returnCode;

    public CompileExceptionError(String message, int returnCode) {
        super(message);
        this.returnCode = returnCode;
    }

    public CompileExceptionError(String message) {
        super(message);
        this.returnCode = 5;
    }

    public int getReturnCode() {
        return returnCode;
    }

}
