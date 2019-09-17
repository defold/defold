package com.dynamo.bob.pipeline;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.BobNLS;

public class BuilderUtil {

    public static String replaceExt(String str, String from, String to) {
        if (str.endsWith(from)) {
            return str.substring(0, str.lastIndexOf(from)).concat(to);
        }
        return str;
    }

    static IResource checkResource(Project project, IResource owner, String field, String path) throws CompileExceptionError {
        if (path.isEmpty()) {
            String message = BobNLS.bind(Messages.BuilderUtil_EMPTY_RESOURCE, field);
            throw new CompileExceptionError(owner, 0, message);
        }
        IResource resource = project.getResource(path);
        if (!resource.exists()) {
            String message = BobNLS.bind(Messages.BuilderUtil_MISSING_RESOURCE, field, path);
            throw new CompileExceptionError(owner, 0, message);
        }
        return resource;
    }

}
