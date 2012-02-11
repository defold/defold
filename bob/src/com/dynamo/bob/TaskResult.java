package com.dynamo.bob;

/**
 * TaskResult. Contains build information for a specifik task execution
 * @author Christian Murray
 *
 */
public class TaskResult {
    private int returnCode;
    private String message = "OK";
    private Task<?> task;

    public TaskResult(Task<?> task) {
        this.task = task;
    }

    /**
     * Set return code. 0 for success. Similar to shell return codes.
     * @param returnCode return code to set
     */
    public void setReturnCode(int returnCode) {
        this.returnCode = returnCode;
    }

    /**
     * Get return code. 0 for success. Similar to shell return codes.
     * @return return code
     */
    public int getReturnCode() {
        return returnCode;
    }

    /**
     * Set informative message. Used primarily for warnings and errors
     * @param message message to set
     */
    public void setMessage(String message) {
        this.message = message;
    }

    /**
     * Get message
     * @return message
     */
    public String getMessage() {
        return message;
    }

    /**
     * Get corresponding tas
     * @return {@link Task}
     */
    public Task<?> getTask() {
        return task;
    }

    @Override
    public String toString() {
        return String.format("%s (%d)", message, returnCode);
    }
}
