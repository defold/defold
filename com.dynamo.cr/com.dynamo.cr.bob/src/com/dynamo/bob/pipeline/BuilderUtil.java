package com.dynamo.bob.pipeline;

import java.io.File;

import org.eclipse.osgi.util.NLS;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IResource;
import com.dynamo.bob.Project;

public class BuilderUtil {

    static String replaceExt(String str, String from, String to) {
        if (str.endsWith(from)) {
            return str.substring(0, str.lastIndexOf(from)).concat(to);
        }
        return str;
    }

    static File checkFile(Project project, IResource owner, String field, String path) throws CompileExceptionError {
        if (path.isEmpty()) {
            String message = NLS.bind(Messages.BuilderUtil_EMPTY_RESOURCE, field);
            throw new CompileExceptionError(owner, 0, message);
        }
        IResource resource = project.getResource(path);
        File file = new File(resource.getAbsPath());
        if (!file.exists()) {
            String message = NLS.bind(Messages.BuilderUtil_MISSING_RESOURCE, field, path);
            throw new CompileExceptionError(owner, 0, message);
        }
        return file;
    }

}
