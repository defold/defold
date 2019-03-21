package com.dynamo.bob.bundle;

import java.io.File;
import java.io.IOException;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;

public interface IBundler {

    void bundleApplication(Project project, File bundleDirectory, ICanceled canceled) throws IOException, CompileExceptionError;

}
