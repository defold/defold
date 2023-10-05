// Copyright 2020-2023 The Defold Foundation
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

    private String relativeFilepath;
    private ResourceNode parent = null;
    private IResource resource;
    private String hexDigest = null;
    private boolean excluded = false;
    protected int useCount = 0;
    protected int excludeCount = 0;
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

    public void increaseUseCount() {
        useCount++;
    }

    public void increaseExcludeCount() {
        excludeCount++;
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
        return relativeFilepath;
    }

    public String getHexDigest() {
        return hexDigest;
    }

    public void setHexDigest(String hexDigest) {
        this.hexDigest = hexDigest;
    }

    public boolean isFullyExcluded() {
        return useCount == excludeCount;
    }

    public boolean getExcludedFlag() {
        return excluded;
    }

    public void setExcludedFlag(boolean excluded) {
        this.excluded = excluded;
    }

    public int getUseCount() {
        return useCount;
    }

    public int getExcludeCount() {
        return excludeCount;
    }

    @Override
    public String toString() {
        return "[ResourceNode path: " + relativeFilepath + "]";
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