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
        public IResource resource;
        public int lineNumber;
        public String message;

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

    public List<Info> errors = new ArrayList<>();
    public List<Info> warnings = new ArrayList<>();

    public MultipleCompileException(String message, Throwable e) {
        super(message, e);
        this.errors.addAll(errors);
    }

    public void addError(IResource resource, String message, int lineNumber) {
        Info info = new Info();
        info.resource = resource;
        info.lineNumber = lineNumber;
        info.message = message;
        this.errors.add( info );
    }

    public void addWarning(IResource resource, String message, int lineNumber) {
        Info info = new Info();
        info.resource = resource;
        info.lineNumber = lineNumber;
        info.message = message;
        this.warnings.add( info );
    }
}
