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

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public final class FolderPackageProvider implements PackageProvider {
    @Override
    public String getId() { return "folder"; }

    @Override
    public String getDisplayName() { return "Folder"; }

    @Override
    public String getCacheKey() { return "folder-v1"; }

    @Override
    public List<Artifact> describeOutputs(Context context, PackageResolver.Package pkg) {
        return List.of(new Artifact(pkg.description().getName(), true));
    }

    @Override
    public void write(Context context, PackageResolver.Package pkg, List<PreparedResource> resources,
                      byte[] manifest, Path stagingDirectory) throws IOException {
        Path output = Files.createDirectory(stagingDirectory.resolve(pkg.description().getName()));
        Set<String> written = new HashSet<>();
        for (PreparedResource resource : resources) {
            context.checkCanceled();
            if (!written.add(resource.info().hash())) continue;
            try (InputStream input = resource.openStream()) { Files.copy(input, output.resolve(resource.info().hash())); }
        }
        Files.write(output.resolve("liveupdate.game.dmanifest"), manifest);
    }
}
