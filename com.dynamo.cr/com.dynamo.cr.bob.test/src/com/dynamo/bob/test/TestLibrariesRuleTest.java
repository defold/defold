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

package com.dynamo.bob.test;

import com.dynamo.bob.util.Library;
import org.junit.Test;

import java.nio.file.Files;
import java.nio.file.Path;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

public class TestLibrariesRuleTest {
    // Overlapping rules must not delete each other's archives when one test finishes first.
    @Test
    public void testOverlappingRulesKeepIndependentFixtures() throws Throwable {
        TestLibrariesRule first = new TestLibrariesRule();
        TestLibrariesRule second = new TestLibrariesRule();
        first.before();
        try {
            second.before();
            try {
                Path firstDirectory = Path.of(first.getServerLocation());
                Path secondDirectory = Path.of(second.getServerLocation());

                first.after();

                assertFalse(Files.exists(firstDirectory));
                assertTrue(Files.isDirectory(secondDirectory));
                assertEquals("0123456789abcdef0123456789abcdef01234567",
                        Library.readArchive(secondDirectory.resolve("test_lib1.zip")).zipComment());
            } finally {
                second.after();
            }
            assertFalse(Files.exists(Path.of(second.getServerLocation())));
        } finally {
            first.after();
        }
    }
}
