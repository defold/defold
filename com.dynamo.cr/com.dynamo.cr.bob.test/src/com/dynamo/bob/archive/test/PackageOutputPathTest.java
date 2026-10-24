// Copyright 2020-2026 The Defold Foundation
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

package com.dynamo.bob.archive.test;

import com.dynamo.bob.archive.liveupdate.PackageOutputPath;
import java.nio.file.Path;
import java.util.Map;
import org.junit.Test;
import static org.junit.Assert.*;

public class PackageOutputPathTest {
    // Paths normalize relative to the project, expand all supported variable forms once, and preserve spaces and literal values.
    @Test
    public void resolvesOutputDirectories() {
        Path root = Path.of("project").toAbsolutePath();
        Map<String, String> environment = Map.of("ROOT", root.resolve("external files").toString(), "NAME", "release$1", "NESTED", "$NAME");
        assertEquals(root.resolve("packages"), PackageOutputPath.resolve("build/../packages", root, environment::get));
        assertEquals(root.resolve("external files/release$1"), PackageOutputPath.resolve("${ROOT}/$NAME", root, environment::get));
        assertEquals(root.resolve("external files/release$1"), PackageOutputPath.resolve("%ROOT%/%NAME%", root, environment::get));
        assertEquals(root.resolve("$NAME"), PackageOutputPath.resolve("$NESTED", root, environment::get));
        assertEquals(root, PackageOutputPath.resolve(root.toString(), root.resolve("nested"), environment::get));
    }

    // Missing variables and empty directories report authoring errors instead of creating literal placeholder directories.
    @Test
    public void rejectsUnresolvedOutputDirectories() {
        for (String path : new String[]{"$MISSING", "${MISSING}", "%MISSING%", "", " "}) {
            try {
                PackageOutputPath.resolve(path, Path.of("project"), name -> null);
                fail("Invalid output directory accepted: " + path);
            } catch (IllegalArgumentException expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("output directory"));
            }
        }
    }
}
