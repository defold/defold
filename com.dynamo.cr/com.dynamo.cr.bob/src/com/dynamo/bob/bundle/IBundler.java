package com.dynamo.bob.bundle;

import java.io.File;
import java.io.IOException;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;

public interface IBundler {

    public void bundleApplication(Project project, File bundleDirectory) throws IOException, CompileExceptionError;

}
