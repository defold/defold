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
import java.nio.file.Path;
import java.util.List;

/** Editor integration fixture: discovered from a pipeline JAR, including during preview. */
public final class ManifestTestProvider implements PackageProvider {
    public String getId() { return "manifest-test"; }
    public String getDisplayName() { return "Custom Archive"; }
    public List<Property> getProperties() {
        return List.of(new Property("extension", "Extension", PropertyType.STRING, "custom", List.of("custom", "alternate")),
                new Property("quality", "Quality", PropertyType.INTEGER, "2", List.of()),
                new Property("scale", "Scale", PropertyType.NUMBER, "1.0", List.of()));
    }
    public List<Artifact> describeOutputs(Context context, PackageResolver.Package pkg) {
        var values = context.providerSettings().get(getId());
        return List.of(new Artifact(pkg.description().getName() + "." + values.get("extension")
                + "-" + values.get("quality") + "-" + values.get("scale"), false));
    }
    public void write(Context context, PackageResolver.Package pkg, List<PreparedResource> resources, byte[] manifest, Path directory) {
        throw new AssertionError("Preview must not call provider writers");
    }
}
