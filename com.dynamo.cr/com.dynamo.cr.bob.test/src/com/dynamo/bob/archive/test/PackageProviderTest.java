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

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IClassScanner;
import com.dynamo.bob.archive.liveupdate.PackageProvider;
import com.dynamo.bob.archive.liveupdate.PackageProviders;
import com.dynamo.bob.archive.liveupdate.PackageResolver;
import java.io.File;
import java.nio.file.Path;
import java.util.List;
import java.util.Set;
import org.junit.Test;
import static org.junit.Assert.*;

public class PackageProviderTest {
    public static class Fixture implements PackageProvider {
        static List<Property> properties = List.of();
        public String getId() { return "fixture"; }
        public String getDisplayName() { return "Fixture Settings"; }
        public List<Property> getProperties() { return properties; }
        public List<Artifact> describeOutputs(Context context, PackageResolver.Package pkg) { throw new AssertionError("Schema discovery must not describe artifacts"); }
        public void write(Context context, PackageResolver.Package pkg, List<PreparedResource> resources, byte[] manifest, Path directory) { throw new AssertionError("Schema discovery must not write artifacts"); }
    }

    private IClassScanner scanner() {
        return new IClassScanner() {
            public Set<String> scan(String pkg) { return Set.of(Fixture.class.getName()); }
            public ClassLoader getClassLoader() { return getClass().getClassLoader(); }
            public void addUrl(File file) { throw new UnsupportedOperationException(); }
        };
    }

    // The editor and Bob share typed parsing, including strict booleans, integer overflow and non-finite number errors.
    @Test
    public void propertyTypesAndChoices() {
        var string = new PackageProvider.Property("text", "Text", PackageProvider.PropertyType.STRING, "", List.of());
        var bool = new PackageProvider.Property("flag", "Flag", PackageProvider.PropertyType.BOOLEAN, "false", List.of());
        var integer = new PackageProvider.Property("count", "Count", PackageProvider.PropertyType.INTEGER, "0", List.of());
        var number = new PackageProvider.Property("scale", "Scale", PackageProvider.PropertyType.NUMBER, "1", List.of());
        var choice = new PackageProvider.Property("quality", "Quality", PackageProvider.PropertyType.INTEGER, "1", List.of("1", "3"));
        assertEquals("${HOME}/output", string.parse("${HOME}/output"));
        assertEquals(false, bool.parse("false"));
        assertEquals(5L, integer.parse("5"));
        assertEquals(0.5, number.parse("0.5"));
        assertEquals(3L, choice.parse("3"));
        List<PackageProvider.Property> properties = List.of(bool, integer, integer, number, number, choice);
        List<String> invalid = List.of("yes", "2.5", "9223372036854775808", "NaN", "Infinity", "2");
        for (int i = 0; i < properties.size(); ++i) {
            try {
                properties.get(i).parse(invalid.get(i));
                fail("Invalid value accepted: " + invalid.get(i));
            } catch (IllegalArgumentException expected) {
                assertTrue(expected.getMessage().contains(properties.get(i).id()));
                assertTrue(expected.getMessage().contains(invalid.get(i)));
            }
        }
    }

    // Schema discovery uses pipeline plugins and rejects malformed descriptions before any output operation.
    @Test
    public void schemaDiscoveryAndValidation() throws Exception {
        var property = new PackageProvider.Property("quality", "Quality", PackageProvider.PropertyType.INTEGER, "1", List.of("1", "3"));
        Fixture.properties = List.of(property);
        var schemas = PackageProviders.describe(scanner());
        assertEquals("Fixture Settings", schemas.get("fixture").label());
        assertEquals(List.of(property), schemas.get("fixture").properties());
        var zip = schemas.get("zip").properties().get(0);
        assertEquals("1", PackageProviders.defaultValue("zip", zip, null));
        assertEquals("9", PackageProviders.defaultValue("zip", zip, "9"));
        for (var invalid : List.of(List.of(property, property),
                List.of(new PackageProvider.Property("quality", "Quality", PackageProvider.PropertyType.INTEGER, "x", List.of())),
                List.of(new PackageProvider.Property("quality", "Quality", PackageProvider.PropertyType.INTEGER, "1", List.of("1", "x"))))) {
            Fixture.properties = invalid;
            try {
                PackageProviders.describe(scanner());
                fail("Invalid schema accepted");
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage().contains("fixture"));
                assertTrue(expected.getMessage().contains("quality"));
            }
        }
        Fixture.properties = List.of();
    }
}
