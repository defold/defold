package com.dynamo.bob.bundle;

import java.io.File;
import java.io.IOException;
import java.util.logging.Logger;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;

public class AndroidBundler implements IBundler {
    private static Logger logger = Logger.getLogger(AndroidBundler.class.getName());

    @Override
    public void bundleApplication(Project project, File bundleDir, ICanceled canceled) throws IOException, CompileExceptionError {
        if (project.hasOption("create-aab")) {
            AndroidAAB.create(project, bundleDir, canceled);
        }
        else {
            AndroidAPK.create(project, bundleDir, canceled);
        }
    }
}
