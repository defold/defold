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

package com.dynamo.bob.archive.liveupdate;

import java.io.BufferedOutputStream;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

/** The built-in containers share payload names, deduplication, and package-local manifests. */
enum BuiltinPackageProvider implements PackageProvider {
    ZIP, FOLDER;

    @Override
    public String getId() { return name().toLowerCase(Locale.ROOT); }

    @Override
    public String getDisplayName() { return this == ZIP ? "ZIP" : "Folder"; }

    @Override
    public List<Property> getProperties() {
        return this == ZIP ? List.of(new Property("compression-level", "Compression Level", PropertyType.INTEGER, "1",
                List.of("-1", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"))) : List.of();
    }

    @Override
    public String getCacheKey() { return getId() + "-v1"; }

    @Override
    public List<Artifact> describeOutputs(Context context, PackageResolver.Package pkg) {
        return List.of(new Artifact(pkg.description().getName() + (this == ZIP ? ".zip" : ""), this == FOLDER));
    }

    @Override
    public void write(Context context, PackageResolver.Package pkg, List<PreparedResource> resources,
                      byte[] manifest, Path stagingDirectory) throws IOException {
        Path output = stagingDirectory.resolve(describeOutputs(context, pkg).getFirst().name());
        if (this == FOLDER) Files.createDirectory(output);
        try (ZipOutputStream zip = this == ZIP ? new ZipOutputStream(new BufferedOutputStream(Files.newOutputStream(output))) : null) {
            if (zip != null) zip.setLevel(((Number) context.providerSettings().get(getId()).get("compression-level")).intValue());
            Set<String> written = new HashSet<>();
            for (PreparedResource resource : resources) {
                context.checkCanceled();
                if (!written.add(resource.info().hash())) continue;
                try (InputStream input = resource.openStream()) {
                    writeEntry(output, zip, resource.info().hash(), input);
                }
            }
            writeEntry(output, zip, "liveupdate.game.dmanifest", new ByteArrayInputStream(manifest));
        }
    }

    private static void writeEntry(Path output, ZipOutputStream zip, String name, InputStream input) throws IOException {
        if (zip == null) {
            Files.copy(input, output.resolve(name));
        } else {
            ZipEntry entry = new ZipEntry(name);
            entry.setTime(0);
            zip.putNextEntry(entry);
            input.transferTo(zip);
            zip.closeEntry();
        }
    }
}
