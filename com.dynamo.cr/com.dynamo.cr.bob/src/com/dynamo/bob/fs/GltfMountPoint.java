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

package com.dynamo.bob.fs;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.SortedSet;
import java.util.TreeSet;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.fs.IFileSystem.IWalker;

/**
 * Mount point that projects materials, images, and meshes from glTF files into Bob's
 * ordinary resource namespace.
 */
public class GltfMountPoint implements IMountPoint {

    private final IFileSystem fileSystem;
    private final SortedSet<String> containerPaths = new TreeSet<String>();
    private final Map<String, GltfContainer> containers = new HashMap<String, GltfContainer>();
    private final Map<String, String> errors = new HashMap<String, String>();
    private final Set<String> loadingContainers = new HashSet<String>();
    private boolean mounted;
    private boolean discovering;

    public GltfMountPoint(IFileSystem fileSystem) {
        if (fileSystem == null) {
            throw new IllegalArgumentException("fileSystem cannot be null");
        }
        this.fileSystem = fileSystem;
    }

    @Override
    public synchronized IResource get(String path) {
        if (!mounted) {
            return null;
        }

        String normalizedPath = normalizePath(path);
        if (isBuildPath(normalizedPath)) {
            return null;
        }

        VirtualPath virtualPath = splitVirtualPath(normalizedPath);
        if (virtualPath == null) {
            return null;
        }

        try {
            GltfContainer container = getContainer(virtualPath.containerPath);
            return container.getResource(virtualPath.childPath);
        } catch (IOException e) {
            errors.put(virtualPath.containerPath, e.getMessage());
            return null;
        }
    }

    public synchronized GltfContainer getContainer(String path) throws IOException {
        if (!mounted) {
            throw new IOException("glTF mount point is not mounted");
        }

        String normalizedPath = normalizePath(path);
        if (!isGltfPath(normalizedPath)) {
            throw new IOException(String.format("Not a glTF resource: '%s'", path));
        }
        if (isBuildPath(normalizedPath)) {
            throw new IOException(String.format("Build output is not a glTF source: '%s'", path));
        }
        if (!loadingContainers.add(normalizedPath)) {
            throw new IOException(String.format("Cyclic glTF container reference involving '%s'", normalizedPath));
        }

        try {
            GltfContainer container = containers.get(normalizedPath);
            if (container != null && !container.isStale(fileSystem)) {
                errors.remove(normalizedPath);
                return container;
            }
            containers.remove(normalizedPath);

            IResource sourceResource = fileSystem.get(normalizedPath);
            if (sourceResource == null || !sourceResource.exists() || !sourceResource.isFile()) {
                containerPaths.remove(normalizedPath);
                throw new IOException(String.format("glTF source does not exist: '%s'", normalizedPath));
            }

            container = GltfContainer.load(fileSystem, sourceResource);
            containers.put(normalizedPath, container);
            containerPaths.add(normalizedPath);
            errors.remove(normalizedPath);
            return container;
        } finally {
            loadingContainers.remove(normalizedPath);
        }
    }

    public synchronized Map<String, String> getErrors() {
        return new HashMap<String, String>(errors);
    }

    @Override
    public synchronized void mount() {
        if (mounted) {
            return;
        }

        containerPaths.clear();
        containers.clear();
        errors.clear();
        loadingContainers.clear();

        refreshContainerPaths();
        mounted = true;
    }

    @Override
    public synchronized void unmount() {
        mounted = false;
        containerPaths.clear();
        containers.clear();
        errors.clear();
        loadingContainers.clear();
    }

    @Override
    public void walk(String path, IWalker walker, Collection<String> results) {
        List<String> paths;
        synchronized (this) {
            if (!mounted || discovering) {
                return;
            }
            refreshContainerPaths();
            paths = new ArrayList<String>(containerPaths);
        }

        String normalizedWalkPath = normalizeWalkPath(path);
        for (String containerPath : paths) {
            if (!intersects(containerPath, normalizedWalkPath)) {
                continue;
            }
            if (containerPath.equals(normalizedWalkPath) && !walker.handleDirectory(containerPath, results)) {
                continue;
            }

            final GltfContainer container;
            try {
                container = getContainer(containerPath);
            } catch (IOException e) {
                synchronized (this) {
                    errors.put(containerPath, e.getMessage());
                }
                continue;
            }

            walkGroup(container, GltfResource.Kind.MATERIAL, "materials", normalizedWalkPath, walker, results);
            walkGroup(container, GltfResource.Kind.IMAGE, "images", normalizedWalkPath, walker, results);
            walkGroup(container, GltfResource.Kind.MESH, "meshes", normalizedWalkPath, walker, results);
        }
    }

    private void refreshContainerPaths() {
        if (discovering) {
            return;
        }

        discovering = true;
        final SortedSet<String> discoveredPaths = new TreeSet<String>();
        try {
            fileSystem.walk("", new FileSystemWalker() {
                @Override
                public void handleFile(String path, Collection<String> results) {
                    String normalizedPath = normalizePath(path);
                    if (!isGltfPath(normalizedPath) || isBuildPath(normalizedPath)) {
                        return;
                    }

                    IResource resource = fileSystem.get(normalizedPath);
                    if (resource != null && resource.exists() && resource.isFile()) {
                        discoveredPaths.add(normalizedPath);
                    }
                }
            }, new ArrayList<String>());
        } finally {
            discovering = false;
        }

        containerPaths.clear();
        containerPaths.addAll(discoveredPaths);
        containers.keySet().retainAll(discoveredPaths);
        errors.keySet().retainAll(discoveredPaths);
    }

    private boolean isBuildPath(String path) {
        String buildDirectory = normalizePath(fileSystem.getBuildDirectory());
        return !buildDirectory.isEmpty() && (path.equals(buildDirectory) || path.startsWith(buildDirectory + "/"));
    }

    private static void walkGroup(GltfContainer container, GltfResource.Kind kind, String groupName, String walkPath,
                                  IWalker walker, Collection<String> results) {
        String groupPath = container.getSourceResource().getPath() + "/" + groupName;
        if (!intersects(groupPath, walkPath)) {
            return;
        }

        boolean hasResources = false;
        for (GltfResource resource : container.getResources()) {
            if (resource.getKind() == kind) {
                hasResources = true;
                break;
            }
        }
        boolean visitGroup = groupPath.equals(walkPath) || container.getSourceResource().getPath().equals(walkPath);
        if (!hasResources || (visitGroup && !walker.handleDirectory(groupPath, results))) {
            return;
        }

        for (GltfResource resource : container.getResources()) {
            if (resource.getKind() == kind && isWithin(resource.getPath(), walkPath)) {
                walker.handleFile(resource.getPath(), results);
            }
        }
    }

    private static boolean intersects(String resourcePath, String walkPath) {
        return isWithin(resourcePath, walkPath) || isWithin(walkPath, resourcePath);
    }

    private static boolean isWithin(String resourcePath, String walkPath) {
        if (walkPath.isEmpty()) {
            return true;
        }
        return resourcePath.equals(walkPath) || resourcePath.startsWith(walkPath + "/");
    }

    private static String normalizeWalkPath(String path) {
        if (path == null || path.isEmpty() || ".".equals(path)) {
            return "";
        }
        String normalizedPath = normalizePath(path);
        while (normalizedPath.endsWith("/")) {
            normalizedPath = normalizedPath.substring(0, normalizedPath.length() - 1);
        }
        return normalizedPath;
    }

    private static String normalizePath(String path) {
        String normalizedPath = FilenameUtils.normalize(path, true);
        if (normalizedPath == null) {
            return "";
        }
        while (normalizedPath.startsWith("/")) {
            normalizedPath = normalizedPath.substring(1);
        }
        return normalizedPath;
    }

    private static boolean isGltfPath(String path) {
        String extension = FilenameUtils.getExtension(path).toLowerCase(Locale.ROOT);
        return "gltf".equals(extension) || "glb".equals(extension);
    }

    private static final class VirtualPath {
        final String containerPath;
        final String childPath;

        VirtualPath(String containerPath, String childPath) {
            this.containerPath = containerPath;
            this.childPath = childPath;
        }
    }

    private static VirtualPath splitVirtualPath(String path) {
        String normalizedPath = normalizePath(path);
        String lowerPath = normalizedPath.toLowerCase(Locale.ROOT);
        int gltfContainerEnd = lowerPath.lastIndexOf(".gltf/");
        int glbContainerEnd = lowerPath.lastIndexOf(".glb/");
        if (gltfContainerEnd >= 0) {
            gltfContainerEnd += 5;
        }
        if (glbContainerEnd >= 0) {
            glbContainerEnd += 4;
        }

        int containerEnd = Math.max(gltfContainerEnd, glbContainerEnd);
        if (containerEnd < 0 || containerEnd + 1 >= normalizedPath.length()) {
            return null;
        }

        String containerPath = normalizedPath.substring(0, containerEnd);
        String childPath = normalizedPath.substring(containerEnd + 1);
        return new VirtualPath(containerPath, childPath);
    }
}
