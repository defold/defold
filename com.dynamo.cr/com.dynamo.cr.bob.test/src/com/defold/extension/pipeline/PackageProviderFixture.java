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

package com.defold.extension.pipeline;

import com.dynamo.bob.archive.liveupdate.PackageProvider;
import com.dynamo.bob.archive.liveupdate.PackageResolver;
import com.dynamo.liveupdate.proto.Manifest;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

/** Test extension exercises the same discovery mechanism as installed pipeline plugins. */
public final class PackageProviderFixture implements PackageProvider {
    public static Context lastContext;
    public static Context lastDescriptionContext;
    public static Manifest.ManifestData lastManifest;
    public static boolean fail;
    public static boolean collide;
    public static int writes;
    public static Runnable beforeWrite;
    public static String cacheKey = "fixture-v1";

    @Override
    public String getId() { return "test-package"; }

    @Override
    public String getDisplayName() { return "Test Package"; }

    @Override
    public List<Property> getProperties() {
        return List.of(new Property("extension", "Extension", PropertyType.STRING, "custom", List.of("custom", "alternate")),
                new Property("quality", "Quality", PropertyType.INTEGER, "2", List.of()),
                new Property("scale", "Scale", PropertyType.NUMBER, "1.0", List.of()));
    }

    @Override
    public String getCacheKey() { return cacheKey; }

    @Override
    public List<Artifact> describeOutputs(Context context, PackageResolver.Package pkg) {
        lastDescriptionContext = context;
        return List.of(new Artifact(collide ? "collision.custom" : pkg.description().getName() + "." + context.providerSettings().get(getId()).get("extension"), false),
                new Artifact("metadata/" + pkg.description().getName() + ".json", false));
    }

    @Override
    public void write(Context context, PackageResolver.Package pkg, List<PreparedResource> resources,
                      byte[] manifest, Path stagingDirectory) throws IOException {
        if (beforeWrite != null) beforeWrite.run();
        lastContext = context;
        lastManifest = Manifest.ManifestData.parseFrom(Manifest.ManifestFile.parseFrom(manifest).getData());
        ++writes;
        try (var output = Files.newOutputStream(stagingDirectory.resolve(describeOutputs(context, pkg).get(0).name()))) {
            output.write(manifest);
            for (PreparedResource resource : resources) {
                try (var input = resource.openStream()) { input.transferTo(output); }
            }
        }
        Path metadata = stagingDirectory.resolve(describeOutputs(context, pkg).get(1).name());
        Files.createDirectories(metadata.getParent());
        Files.writeString(metadata, "{\"package\":\"" + pkg.description().getName() + "\"}");
        if (fail) throw new IOException("Deliberate provider failure");
    }
}
