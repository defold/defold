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

package com.dynamo.bob.util;

import org.junit.Test;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import static org.junit.Assert.assertEquals;

public class LibraryTest {

    @Test
    public void testReadArchiveParsesIncludeDirs() throws Exception {
        assertIncludeDirs("", Set.of());
        assertIncludeDirs("foo bar baz", Set.of("foo", "bar", "baz"));
        assertIncludeDirs("foo,bar,baz", Set.of("foo", "bar", "baz"));
        assertIncludeDirs("  foo  bar  baz  ", Set.of("foo", "bar", "baz"));
        assertIncludeDirs("  ,, foo , bar ,,, baz  ", Set.of("foo", "bar", "baz"));
        assertIncludeDirs("\tfoo\t\tbar\tbaz", Set.of("foo", "bar", "baz"));
        assertIncludeDirs("im\\ possible", Set.of("im\\", "possible"));
    }

    private static void assertIncludeDirs(String includeDirs, Set<String> expected) throws Exception {
        var archive = Files.createTempFile("library-test", ".zip");
        try (var output = Files.newOutputStream(archive);
             var zip = new ZipOutputStream(output)) {
            zip.putNextEntry(new ZipEntry("game.project"));
            zip.write(("[library]\ninclude_dirs=" + includeDirs + "\n").getBytes(StandardCharsets.UTF_8));
            zip.closeEntry();
        }
        try {
            assertEquals(expected, Library.readArchive(archive).includeDirs());
        } finally {
            Files.deleteIfExists(archive);
        }
    }

}
