// Copyright 2020 The Defold Foundation
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
        String bundleFormat = project.option("bundle-format", "apk");
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
