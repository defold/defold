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

package com.dynamo.bob.pipeline.graph;

import java.util.Set;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.HashMap;
import java.util.List;
import java.util.LinkedList;
import java.util.Collection;

import java.io.Writer;
import java.io.StringWriter;
import java.io.BufferedWriter;
import java.io.OutputStreamWriter;
import java.io.OutputStream;
import java.io.IOException;

import org.apache.commons.io.IOUtils;

import org.codehaus.jackson.JsonFactory;
import org.codehaus.jackson.JsonGenerator;

import com.dynamo.bob.Project;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.graph.ResourceWalker;
import com.dynamo.bob.pipeline.graph.ResourceWalker.IResourceVisitor;
import com.dynamo.gamesys.proto.GameSystem.CollectionProxyDesc;

import com.google.protobuf.Message;

public class ResourceGraph implements IResourceVisitor {

    private Project project;

    // set of all resources in the graph
    private Set<IResource> resources = new HashSet<>();

    // set of all resource nodes in the graph
    private Set<ResourceNode> resourceNodes = new LinkedHashSet<>();

    // lookup between IResource and ResourceNode
    private Map<IResource, ResourceNode> resourceToNodeLookup = new HashMap<>();

    // lookup between resource path and ResourceNode
    private Map<String, ResourceNode> pathToNodeLookup = new HashMap<>();

    // root resource to which all other resources are added
    private ResourceNode root = new ResourceNode("<AnonymousRoot>");

    public ResourceGraph(Project project) {
        this.project = project;
        root.flagAsUsedInMainBundle();
    }

    @Override
    public boolean shouldVisit(IResource resource, IResource parentResource) {
        ResourceNode currentNode = resourceToNodeLookup.get(resource);
        if (currentNode != null) {
            // the node has already been visited which means that it is used in
            // multiple places in the graph
            // in this case we reuse the node and don't visit it again
            ResourceNode parentNode = (parentResource != null) ? resourceToNodeLookup.get(parentResource) : root;
            addNodeToParent(parentNode, currentNode);
            return false;
        }
        return true;
    }

    @Override
    public void visit(IResource resource, IResource parentResource) throws CompileExceptionError {
        IResource output = resource.output();
        resources.add(output);

        // create a new node since we are only visiting new resources (see check in shouldVisit)
        ResourceNode currentNode = new ResourceNode(resource);
        resourceNodes.add(currentNode);

        // add resource node to lookup tables
        resourceToNodeLookup.put(resource, currentNode);
        pathToNodeLookup.put("/" + resource.getPath(), currentNode);

        // add resource node to graph
        ResourceNode parentNode = (parentResource != null) ? resourceToNodeLookup.get(parentResource) : root;
        addNodeToParent(parentNode, currentNode);
    }

    @Override
    public void visitMessage(Message message, IResource resource, IResource parentResource) throws CompileExceptionError {
        if (message instanceof CollectionProxyDesc) {
            CollectionProxyDesc desc = (CollectionProxyDesc)message;
            ResourceNode collectionProxyNode = resourceToNodeLookup.get(resource);
            if (desc.getExclude()) {
                collectionProxyNode.setType(ResourceNode.Type.ExcludedCollectionProxy);
            }
            else {
                collectionProxyNode.setType(ResourceNode.Type.CollectionProxy);
            }
        }
    }

    @Override
    public void leave(IResource resource, IResource parentResource) throws CompileExceptionError {
        // do nothing
    }

    /**
     * Add a resource to the graph. This will add the resource and all sub-resources
     * to the graph.
     * @param rootResource The resource to create graph from.
     */
    public void add(IResource rootResource) throws CompileExceptionError {
        ResourceWalker.walk(project, rootResource, this);
    }

    private void addNodeToParent(ResourceNode parentNode, ResourceNode childNode) {
        if (parentNode.checkType(ResourceNode.Type.ExcludedCollectionProxy)) {
            childNode.setType(ResourceNode.Type.ExcludedCollection);
        }
        parentNode.addChild(childNode);
    }

    // used in tests
    public ResourceNode add(String resourcePath, ResourceNode parentNode) {
        IResource currentResource = project.getResource(resourcePath);
        IResource parentResource = project.getResource(parentNode.getPath());
        ResourceNode currentNode = resourceToNodeLookup.get(currentResource);
        if (currentNode == null) {
            currentNode = new ResourceNode(resourcePath);
            resourceToNodeLookup.put(currentResource, currentNode);
            pathToNodeLookup.put("/" + currentResource.getPath(), currentNode);
            resourceNodes.add(currentNode);
            if (currentResource.getPath().endsWith("collectionproxyc")) {
                currentNode.setType(ResourceNode.Type.CollectionProxy);
            }
        }
        addNodeToParent(parentNode, currentNode);
        return currentNode;
    }
    public ResourceNode add(ResourceNode resourceNode, ResourceNode parentNode) {
        return add(resourceNode.getPath(), parentNode);
    }

    /**
     * Get the root resource node of the graph. All resources added to the graph
     * will exist as children of the root resource.
     * @return The root node
     */
    public ResourceNode getRootNode() {
        return root;
    }

    /**
     * Get a collection of all resources added to the graph.
     * @return A collection of resources
     */
    public Collection<IResource> getResources() {
        return resources;
    }

    /**
     * Get resource node from path
     * @param path The path to get resource node for
     * @return The resource node for the path
     */
    public ResourceNode getResourceNodeFromPath(String path) {
        return pathToNodeLookup.get(path);
    }

    /**
     * Set hex digests for all resource nodes in the graph
     * @param hexDigests Map with hex digests, keyed on relative resource paths
     */
    public void setHexDigests(Map<String, String> hexDigests) {
        for (ResourceNode resourceNode : resourceNodes) {
            String hexDigest = hexDigests.get(resourceNode.getPath());
            resourceNode.setHexDigest(hexDigest);
        }
    }

    private void flagAllNonExcludedResources(ResourceNode node, HashSet<ResourceNode> visited) {
        for (ResourceNode child : node.getChildren()) {
            if (visited.contains(child)) {
                continue;
            }
            child.flagAsUsedInMainBundle();
            visited.add(child);
            if (child.checkType(ResourceNode.Type.ExcludedCollectionProxy)) {
                continue;
            }
            flagAllNonExcludedResources(child, visited);
        }
    }

    /**
     * Flag all resource nodes used in the main resource bundle.
     * Resource nodes without this flag can be excluded from the main archive 
     * and moved to a live update archive
     */
    public void findAllResourcesReferencedFromMainCollection() {
        flagAllNonExcludedResources(root, new HashSet<ResourceNode>());
    }

    /**
     * Create a list of all excluded resources. A resource is considered to be
     * excluded if it is only referenced from excluded collection proxies. If a
     * resource is referenced both from an excluded collection proxy and from
     * a non-excluded collection it will not be considered an excluded resource.
     * @return List of excluded resources
     */
    public List<String> createExcludedResourcesList() {
        findAllResourcesReferencedFromMainCollection();
        List<String> excludedResources = new LinkedList<>();
        for (ResourceNode node : resourceNodes) {
            if (node.isInMainBundle()) {
                continue;
            }
            excludedResources.add(node.getPath());
        }
        return excludedResources;
    }

    public void writeJSON(ResourceNode node, JsonGenerator generator, boolean shouldPublishLU) throws IOException {
        generator.writeStartObject();
        generator.writeFieldName("path");
        generator.writeString(node.getPath());

        generator.writeFieldName("hexDigest");
        generator.writeString(node.getHexDigest());

        if (!node.checkType(ResourceNode.Type.None)) {
            generator.writeFieldName("nodeType");
            generator.writeString(node.getNodeTypeAsString());
        }

        if (shouldPublishLU) {
            boolean isInMainBundle = node.isInMainBundle();
            generator.writeFieldName("isInMainBundle");
            generator.writeBoolean(isInMainBundle);

            if (!isInMainBundle && node.getHexDigest() == null) {
                throw new RuntimeException("Resource '" + node.getPath() + "' has no hex digest");
            }
        }

        generator.writeFieldName("children");
        generator.writeStartArray();

        for (ResourceNode child : node.getChildren()) {
            generator.writeString(child.getPath());
        }
        generator.writeEndArray();
        generator.writeEndObject();
    }

    private void writeJSON(Writer writer) throws IOException {
        boolean shouldPublishLU = project.option("liveupdate", "false").equals("true");
        JsonGenerator generator = null;
        try {
            generator = (new JsonFactory()).createJsonGenerator(writer);
            generator.useDefaultPrettyPrinter();
            generator.writeStartArray();
            writeJSON(root, generator, shouldPublishLU);
            for (ResourceNode node : resourceNodes) {
                writeJSON(node, generator, shouldPublishLU);
            }
            generator.writeEndArray();
        }
        finally {
            if (generator != null) {
                generator.close();
            }
            IOUtils.closeQuietly(writer);
        }
    }

    public void writeJSON(OutputStream os) throws IOException {
        OutputStreamWriter outputStreamWriter = new OutputStreamWriter(os);
        BufferedWriter writer = new BufferedWriter(outputStreamWriter);
        writeJSON(writer);
    }

    public String toJSON() throws IOException {
        StringWriter stringWriter = new StringWriter();
        BufferedWriter writer = new BufferedWriter(stringWriter);
        writeJSON(writer);
        return stringWriter.toString();
    }

}