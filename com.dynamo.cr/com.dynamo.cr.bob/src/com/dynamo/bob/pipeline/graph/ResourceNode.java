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

import org.codehaus.jackson.JsonFactory;
import org.codehaus.jackson.JsonGenerator;
import org.apache.commons.io.IOUtils;

import java.io.IOException;
import java.io.Writer;
import java.io.StringWriter;
import java.io.BufferedWriter;
import java.io.OutputStreamWriter;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

public class ResourceNode {

    private String relativeFilepath;
    private String absoluteFilepath;
    private ResourceNode parent = null;
    private IResource resource;
    private String hexDigest = null;
    private boolean excluded = false;
    private final List<ResourceNode> children = new ArrayList<ResourceNode>();

    public ResourceNode(IResource resource) {
        this(resource.getPath(), resource.output().getAbsPath());
        this.resource = resource;

    }
    public ResourceNode(final String relativeFilepath, final String absoluteFilepath) {
        if (relativeFilepath.startsWith("/")) {
            this.relativeFilepath = relativeFilepath;
        } else {
            this.relativeFilepath = "/" + relativeFilepath;
        }
        this.absoluteFilepath = absoluteFilepath;
    }

    public void addChild(ResourceNode childNode) {
        childNode.parent = this;
        this.children.add(childNode);
    }

    public void addChildren(List<ResourceNode> childNodes) {
        for (ResourceNode childNode : childNodes) {
            childNode.parent = this;
            this.children.add(childNode);
        }
    }

    public List<ResourceNode> getChildren() {
        return this.children;
    }

    public ResourceNode getParent() {
        return this.parent;
    }

    public IResource getResource() {
        return this.resource;
    }

    public String getPath() {
        return relativeFilepath;
    }

    public String getAbsolutePath() {
        return absoluteFilepath;
    }

    public void setHexDigest(String hexDigest) {
        this.hexDigest = hexDigest;
    }

    public void setExcluded(boolean excluded) {
        this.excluded = excluded;
    }

    private void writeJSON(JsonGenerator generator) throws IOException {
        generator.writeStartObject();
        generator.writeFieldName("path");
        generator.writeString(relativeFilepath);

        generator.writeFieldName("hexDigest");
        generator.writeString(hexDigest);

        generator.writeFieldName("excluded");
        generator.writeBoolean(excluded);

        generator.writeFieldName("children");
        generator.writeStartArray();
        for (ResourceNode child : children) {
            child.writeJSON(generator);
        }
        generator.writeEndArray();
        generator.writeEndObject();
    }

    private void writeJSON(Writer writer) throws IOException {
        JsonGenerator generator = null;
        try {
            generator = (new JsonFactory()).createJsonGenerator(writer);
            generator.useDefaultPrettyPrinter();
            writeJSON(generator);
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