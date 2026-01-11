// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob;

/**
 * TaskResult. Contains build information for a specifik task execution
 * @author Christian Murray
 *
 */
public class TaskResult {

    public enum Result {
        SUCCESS,
        RETRY,
        SKIPPED,
        FAILED
    }

    private Result result = Result.SUCCESS;
    private String message = "OK";
    private Task task;
    private Throwable exception;
    private int lineNumber = 0;

    public TaskResult(Task task) {
        this.task = task;
    }

    /**
     * Set the task result.
     * @param result The result
     */
    public void setResult(Result result) {
        this.result = result;
    }

    /**
     * Get the task result.
     * @return result
     */
    public Result getResult() {
        return this.result;
    }

    public boolean isOk() {
        return this.result != Result.FAILED;
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
     * Get corresponding task
     * @return {@link Task}
     */
    public Task getTask() {
        return task;
    }

    @Override
    public String toString() {
        return String.format("%s (%s)", message, this.result);
    }

    /**
     * Set exception. Should only be set, ie not null, when unexpected errors occur
     * @param exception exception to set. null is accepted.
     */
    public void setException(Throwable exception) {
        this.exception = exception;
    }

    /**
     * Get exception. If not null a unexpected error has occurred.
     * @return exception or null if no exception is set.
     */
    public Throwable getException() {
        return exception;
    }

    /**
     * Check if the result contains an exception
     * @return true if an exception happened
     */
    public boolean hasException() {
        return exception != null;
    }

    /**
     * Set line number. Only used when the task failed.
     * @param lineNumber must be positive
     */
    public void setLineNumber(int lineNumber) {
        this.lineNumber = lineNumber;
    }

    /**
     * Get line number. Only relevant when the task failed.
     * @return line number
     */
    public int getLineNumber() {
        return this.lineNumber;
    }
}
