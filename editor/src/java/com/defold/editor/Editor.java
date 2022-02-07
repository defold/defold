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

package com.defold.editor;

import com.defold.util.SupportPath;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

public class Editor {

    public static boolean isDev() {
        return System.getProperty("defold.version") == null;
    }

    public static Path getSupportPath() {
        Path supportPath = SupportPath.getPlatformSupportPath("Defold");
        if (!Files.exists(supportPath)) {
            supportPath.toFile().mkdirs();
        }
        return supportPath;
    }

    public static Path getLogDirectory() {
        String defoldLogDir = System.getProperty("defold.log.dir");
        return defoldLogDir != null ? Paths.get(defoldLogDir) : getSupportPath();
    }
}
