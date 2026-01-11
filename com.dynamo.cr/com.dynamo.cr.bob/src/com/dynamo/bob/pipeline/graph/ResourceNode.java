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

import com.dynamo.bob.fs.IResource;

import java.util.ArrayList;
import java.util.List;

public class ResourceNode {

    public enum Type {
        None,
        CollectionProxy,
        ExcludedCollection,
        ExcludedCollectionProxy
    }

    private String relativeFilepath;
    private ResourceNode parent = null;
    private IResource resource;
    private String hexDigest = null;
    private Type nodeType = Type.None;
    protected boolean inMainBundle = false;
    private final List<ResourceNode> children = new ArrayList<ResourceNode>();

    public ResourceNode(IResource resource) {
        this(resource.getPath());
        this.resource = resource;
    }
    public ResourceNode(final String relativeFilepath) {
        if (relativeFilepath.startsWith("/")) {
            this.relativeFilepath = relativeFilepath;
        } else {
            this.relativeFilepath = "/" + relativeFilepath;
        }
    }

    public void addChild(ResourceNode childNode) {
        this.children.add(childNode);
    }

    public void addChildren(List<ResourceNode> childNodes) {
        for (ResourceNode childNode : childNodes) {
            addChild(childNode);
        }
    }

    public List<ResourceNode> getChildren() {
        return this.children;
    }

    public IResource getResource() {
        return this.resource;
    }

    public String getPath() {
        return this.relativeFilepath;
    }

    public String getHexDigest() {
        return this.hexDigest;
    }

    public void setHexDigest(String hexDigest) {
        this.hexDigest = hexDigest;
    }

    public boolean isInMainBundle() {
        return this.inMainBundle;
    }

    public void flagAsUsedInMainBundle() {
        this.inMainBundle = true;
    }

    public void setType(Type type) {
        this.nodeType = type;
    }

    public boolean checkType(Type type) {
        // ExcludedCollectionProxy is CollectionProxy but CollectionProxy isn't ExcludedCollectionProxy
        if (this.nodeType == Type.ExcludedCollectionProxy) {
            return type == Type.CollectionProxy || type == Type.ExcludedCollectionProxy;
        }
        return this.nodeType == type;
    }

    public String getNodeTypeAsString() {
        return this.nodeType.name();
    }

    @Override
    public String toString() {
        return "[ResourceNode path: " + relativeFilepath + ", " + super.toString() + "]";
    }

    @Override
    public int hashCode() {
        return relativeFilepath.hashCode();
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof ResourceNode) {
            ResourceNode r = (ResourceNode) obj;
            return this.relativeFilepath.equals(r.relativeFilepath);
        } else {
            return super.equals(obj);
        }
    }
}