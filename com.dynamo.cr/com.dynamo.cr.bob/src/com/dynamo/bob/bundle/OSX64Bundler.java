package com.dynamo.bob.bundle;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.Collection;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.util.BobProjectProperties;

public class OSX64Bundler implements IBundler {
    @Override
    public void bundleApplication(Project project, File bundleDir)
            throws IOException, CompileExceptionError {
        new OSX32Bundler().bundleApplicationForPlatform(Platform.X86_64Darwin, project, bundleDir);
    }
}
