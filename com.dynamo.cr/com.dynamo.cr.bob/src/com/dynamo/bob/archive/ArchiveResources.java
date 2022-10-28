// Copyright 2020-2022 The Defold Foundation
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

package com.dynamo.bob.archive;

import java.io.IOException;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.Set;
import java.util.HashSet;
import java.util.ArrayList;

import com.dynamo.bob.Bob;
import com.dynamo.bob.pipeline.ResourceNode;

public class ArchiveResources {

    // Which resources to include in this archive
    public Set<String> includes = new HashSet<>();
    // Which resources to exclude from this archive
    public Set<String> excludes = new HashSet<>();
    // The final list of resources to include
    public Set<String> resources = new HashSet<>();

    public List<ArchiveResources> includeArchives = new ArrayList<>();
    public List<ArchiveResources> excludeArchives = new ArrayList<>();

    public String archiveName;

    public ArchiveResources(String name) {
        archiveName = name;
    }

    // E.g. "this" is Level1, and it is dependent on other, the "common" asset pack
    private void addIncludeDependency(ArchiveResources other) {
        includeArchives.add(other);
    }
    private void addExcludeDependency(ArchiveResources other) {
        excludeArchives.add(other);
    }

    public void addInclude(String url) {
        includes.add(url);
    }
    public void addExclude(String url) {
        excludes.add(url);
    }

    // For the internal building step

    public boolean isResourceIncluded(String path) {
        return includes.contains(path);
    }
    public boolean isResourceExcluded(String path) {
        return excludes.contains(path);
    }

    public boolean contains(String path) {
        return resources.contains(path);
    }

    public void addResource(String url) {
        resources.add(url);
    }
    public void addResources(ArchiveResources other) {
        resources.addAll(other.resources);
    }
    public void removeResource(String url) {
        resources.remove(url);
    }
    public void removeResources(ArchiveResources other) {
        resources.removeAll(other.resources);
    }
    public void removeResources(Set<String> other) {
        resources.removeAll(other);
    }

}
