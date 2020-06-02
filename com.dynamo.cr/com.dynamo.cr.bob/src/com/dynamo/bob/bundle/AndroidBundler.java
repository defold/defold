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
        String bundleFormat = project.option("bundle-format", "");
        if (bundleFormat.equals("aab")) {
            AndroidAAB.create(project, bundleDir, canceled);
        }
        else if (bundleFormat.equals("apk")) {
            AndroidAPK.create(project, bundleDir, canceled);
        }
        else {
            throw new CompileExceptionError(null, -1, "Unknown bundle format: " + bundleFormat);
        }
    }
}
