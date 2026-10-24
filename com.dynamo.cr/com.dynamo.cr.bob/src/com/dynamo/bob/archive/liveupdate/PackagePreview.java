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

import com.dynamo.bob.ClassLoaderResourceScanner;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IProgress;
import com.dynamo.bob.MultipleCompileException;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.fs.DefaultFileSystem;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Builds a caller-owned snapshot with the same compilers and resolver as bundling, without provider writers. */
public final class PackagePreview {
    public record Result(PackageResolver.Plan plan, Map<String, Long> compiledSizes,
                         Map<String, List<String>> exportPaths) {
        public Result {
            compiledSizes = Map.copyOf(compiledSizes);
            Map<String, List<String>> paths = new LinkedHashMap<>();
            exportPaths.forEach((name, outputs) -> paths.put(name, List.copyOf(outputs)));
            exportPaths = Map.copyOf(paths);
        }
    }

    private static Map<String, List<String>> exportPaths(Project project, Path projectRoot, PackageResolver.Plan plan) throws IOException, CompileExceptionError {
        Map<String, PackageProvider.ResourceInfo> resources = new LinkedHashMap<>();
        for (List<ArchiveEntry> entries : List.of(project.getArchiveBuilder().getIncludedEntries(), project.getArchiveBuilder().getExcludedEntries())) {
            for (ArchiveEntry entry : entries) {
                String path = PackageResolver.normalize(entry.getRelativeFilename());
                resources.put(path, new PackageProvider.ResourceInfo(path, entry.getHexDigest(), entry.getSize(),
                        entry.getCompressedSize(), entry.getFlags() | ArchiveEntry.FLAG_LIVEUPDATE));
            }
        }
        PackageProvider.Context context = PackageWriter.createContext(project, plan, resources);
        Map<String, PackageProvider> providers = PackageWriter.providers(project);
        Path directory = PackageBuilder.outputDirectory(project, projectRoot);
        Map<String, List<String>> paths = new LinkedHashMap<>();
        for (PackageResolver.Package pkg : plan.packages().values()) {
            context.checkCanceled();
            String providerId = pkg.description().getProvider();
            PackageProvider provider = providers.get(providerId);
            if (provider == null) throw new CompileExceptionError("Unknown Live Update provider: " + providerId);
            paths.put(pkg.description().getName(), provider.describeOutputs(context, pkg).stream()
                    .map(artifact -> directory.resolve(artifact.name()).normalize().toString().replace('\\', '/'))
                    .toList());
        }
        return paths;
    }

    public static Result build(Path snapshot, Path projectRoot, String manifestPath, Map<String, String> options,
                               ClassLoader classLoader, IProgress progress) throws IOException, CompileExceptionError, MultipleCompileException {
        try (Project project = new Project(classLoader, new DefaultFileSystem(), snapshot.toString(), "build")) {
            options.forEach(project::setOption);
            project.setOption("platform", options.getOrDefault("platform", Platform.getHostPlatform().getPair()));
            project.setOption("variant", options.getOrDefault("variant", "debug"));
            project.setOption("archive", "true");
            project.setOption("liveupdate", "true");
            project.setOption("package-preview", "true");
            project.setOption("package-manifest", manifestPath);
            // Library resources are already captured in the snapshot by the editor.
            project.mount(new ClassLoaderResourceScanner(), List.of());
            for (TaskResult task : project.build(progress, "build")) {
                if (!task.isOk()) throw new CompileExceptionError(task.getTask().input(0), task.getLineNumber(), task.getMessage(), task.getException());
            }
            BundleHelper.throwIfCanceled(progress);
            PackageResolver.Plan plan = project.getLiveUpdatePlan();
            if (plan == null) throw new IOException("Preview did not produce a Live Update package plan");
            Map<String, Long> sizes = new LinkedHashMap<>();
            for (String path : plan.resources().keySet()) sizes.put(path, Files.size(snapshot.resolve("build").resolve(path.substring(1))));
            return new Result(plan, sizes, exportPaths(project, projectRoot, plan));
        }
    }
}
