package com.dynamo.bob;

import java.util.ArrayList;
import java.util.List;

import com.dynamo.bob.fs.IResource;


/**
 * Compile exception
 * @author Mathias Westerdahl
 *
 */
public class MultipleCompileException extends Exception {
    private static final long serialVersionUID = -3189379067765141096L;

    public class Info {
        public static final int SEVERITY_INFO = 0;
        public static final int SEVERITY_WARNING = 1;
        public static final int SEVERITY_ERROR = 2;

        public int severity;
        public IResource resource;
        public int lineNumber;
        public String message;

        public int getSeverity() {
            return severity;
        }
        public IResource getResource() {
            return resource;
        }
        public String getMessage() {
            return message;
        }
        public int getLineNumber() {
            return lineNumber;
        }
    }

    public final List<Info> issues = new ArrayList<>();
    private IResource contextResource;
    private String rawLog;

    public MultipleCompileException(String message, Throwable e) {
        super(message, e);
    }

    public void addIssue(int severity, IResource resource, String message, int lineNumber) {
        if (severity != Info.SEVERITY_INFO && severity != Info.SEVERITY_WARNING && severity != Info.SEVERITY_ERROR) {
            throw new IllegalArgumentException("severity outside acceptable range.");
        }

        if (resource == null) {
            throw new IllegalArgumentException("resource cannot be null.");
        }

        if (message == null) {
            throw new IllegalArgumentException("message cannot be null.");
        }

        if (lineNumber < 0) {
            throw new IllegalArgumentException("lineNumber cannot be negative.");
        }

        Info info = new Info();
        info.severity = severity;
        info.resource = resource;
        info.lineNumber = lineNumber;
        info.message = message;
        this.issues.add(info);
    }

    public void attachLog(IResource contextResource, String rawLog) {
        if (contextResource == null) {
            throw new IllegalArgumentException("contextResource cannot be null.");
        }

        if (rawLog == null) {
            throw new IllegalArgumentException("rawLog cannot be null.");
        }

        this.contextResource = contextResource;
        this.rawLog = rawLog;
    }

    public String getRawLog() {
        return this.rawLog;
    }

    public IResource getContextResource() {
        return this.contextResource;
    }
}
