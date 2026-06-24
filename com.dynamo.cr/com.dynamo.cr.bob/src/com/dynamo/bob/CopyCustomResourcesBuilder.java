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

package com.dynamo.bob;

import java.io.IOException;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.DependencyMetadata;

public class CopyCustomResourcesBuilder extends Builder {
    public static List<String> getCustomResourcePaths(Project project) {
        BobProjectProperties properties = project.getProjectProperties();
        Set<String> paths = new LinkedHashSet<>();

        String[] resources = properties.getStringArrayValue("project", "custom_resources", new String[0]);
        for (String s : resources) {
            s = s.trim();
            if (s.length() > 0) {
                // Could be directory or file; use findResourcePaths to traverse and grab all.
                project.findResourcePaths(s, paths);
            }
        }

        return new ArrayList<>(paths);
    }

    public static IResource getDependencyMetadataInputResource(Project project) {
        BobProjectProperties properties = project.getProjectProperties();
        if (properties.getBooleanValue("project", "dependencies_metadata", true)) {
            IResource dependenciesMetadata = project.getResource(DependencyMetadata.PROJECT_PATH);
            if (dependenciesMetadata.exists()) {
                return dependenciesMetadata;
            }
        }
        return null;
    }

    public static IResource getDependencyMetadataOutputResource(Project project) {
        return project.getResource(FilenameUtils.concat(project.getBuildDirectory(), DependencyMetadata.OUTPUT_PATH));
    }

    @Override
    public Task create(IResource input) throws CompileExceptionError {
        TaskBuilder b = Task.newBuilder(this)
                .setName("Copy Custom Resources");

        for (String path : getCustomResourcePaths(this.project)) {
            IResource r = this.project.getResource(path);
            b.addInput(r);
            b.addOutput(r.output());
        }
        IResource dependenciesMetadata = getDependencyMetadataInputResource(this.project);
        if (dependenciesMetadata != null) {
            b.addInput(dependenciesMetadata);
            b.addOutput(getDependencyMetadataOutputResource(this.project));
        }
        return b.build();
    }

    @Override
    public void build(Task task) throws IOException {
        final List<IResource> outputs = task.getOutputs();
        final List<IResource> inputs = task.getInputs();
        final int n = inputs.size();
        final String dependencyMetadataOutputPath = getDependencyMetadataOutputResource(this.project).getPath();
        for (int i = 0; i < n; i++) {
            IResource output = outputs.get(i);
            byte[] content = inputs.get(i).getContent();
            if (dependencyMetadataOutputPath.equals(output.getPath())) {
                content = DependencyMetadata.minifyJson(content);
            }
            output.setContent(content);
        }
    }
}
