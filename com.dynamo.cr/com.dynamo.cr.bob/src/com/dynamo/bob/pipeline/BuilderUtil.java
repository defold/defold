// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

    public static String replaceExt(String str, String to) {
        int last_dot = str.lastIndexOf(".");
        if (last_dot != -1) {
            return str.substring(0, last_dot).concat(to);
        }
        return str.concat(to);
    }

    // Returns "dae" from "path/to.dae"
    public static String getSuffix(String path) {
        return path.substring(path.lastIndexOf(".") + 1);
    }

    public static IResource checkResource(Project project, IResource owner, String field, String path) throws CompileExceptionError {
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
