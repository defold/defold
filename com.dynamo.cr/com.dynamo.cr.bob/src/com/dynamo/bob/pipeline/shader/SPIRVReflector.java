// Copyright 2020-2024 The Defold Foundation
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

package com.dynamo.bob.pipeline.shader;

import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;

import java.io.IOException;
import java.util.*;

public class SPIRVReflector {
    private static JsonNode root;

    public SPIRVReflector(String json) throws IOException {
        this.root = (new ObjectMapper()).readTree(json);
    }

    public class ResourceMember {
        public String name;
        public String type;
        public int    elementCount;
        public int    offset;
    }

    public class Resource {
        public String name;
        public String type;
        public int    binding;
        public int    set;
        public int    blockSize;
    }

    public class ResourceType {
        public String                    key;
        public String                    name;
        public ArrayList<ResourceMember> members = new ArrayList<>();
    }

    private class SortBindingsComparator implements Comparator<Resource> {
        public int compare(SPIRVReflector.Resource a, SPIRVReflector.Resource b) {
            return a.binding - b.binding;
        }
    }

    public ArrayList<ResourceType> getTypes() {
        ArrayList<ResourceType> resourceTypes = new ArrayList<>();
        JsonNode typesNode = root.get("types");
        if (typesNode == null) {
            return resourceTypes;
        }

        for (Iterator<Map.Entry<String, JsonNode>> jsonFields = typesNode.getFields(); jsonFields.hasNext();) {
            Map.Entry<String, JsonNode> jsonField = jsonFields.next();
            String key = jsonField.getKey();
            JsonNode value = jsonField.getValue();

            ResourceType type = new ResourceType();
            type.key = key;
            type.name = value.get("name").asText();

            JsonNode membersNode = value.get("members");
            Iterator<JsonNode> membersNodeIt = membersNode.getElements();

            while(membersNodeIt.hasNext()) {
                JsonNode memberNode = membersNodeIt.next();
                ResourceMember res  = new ResourceMember();
                res.name            = memberNode.get("name").asText();
                res.type            = memberNode.get("type").asText();

                JsonNode offsetNode = memberNode.get("offset");
                if (offsetNode != null) {
                    res.offset = offsetNode.asInt();
                }

                JsonNode arrayNode = memberNode.get("array");
                if (arrayNode != null && arrayNode.isArray())
                {
                    res.elementCount = arrayNode.get(0).asInt();
                }

                type.members.add(res);
            }

            resourceTypes.add(type);
        }

        return resourceTypes;
    }

    public ArrayList<Resource> getUBOs() {
        ArrayList<Resource> ubos = new ArrayList<>();
        JsonNode ubosNode = root.get("ubos");

        if (ubosNode == null) {
            return ubos;
        }

        Iterator<JsonNode> uniformBlockNodeIt = ubosNode.getElements();
        while (uniformBlockNodeIt.hasNext()) {
            JsonNode uboNode = uniformBlockNodeIt.next();

            Resource ubo  = new Resource();
            ubo.name      = uboNode.get("name").asText();
            ubo.set       = uboNode.get("set").asInt();
            ubo.binding   = uboNode.get("binding").asInt();
            ubo.type      = uboNode.get("type").asText();
            ubo.blockSize = uboNode.get("block_size").asInt();
            ubos.add(ubo);
        }

        ubos.sort(new SortBindingsComparator());

        return ubos;
    }

    public ArrayList<Resource> getSsbos() {
        ArrayList<Resource> ssbos = new ArrayList<Resource>();

        JsonNode ssboNode  = root.get("ssbos");

        if (ssboNode == null) {
            return ssbos;
        }

        Iterator<JsonNode> ssboBlockIt = ssboNode.getElements();
        while (ssboBlockIt.hasNext()) {
            JsonNode ssboBlockNode = ssboBlockIt.next();

            Resource ssbo  = new Resource();
            ssbo.name      = ssboBlockNode.get("name").asText();
            ssbo.set       = ssboBlockNode.get("set").asInt();
            ssbo.binding   = ssboBlockNode.get("binding").asInt();
            ssbo.type      = ssboBlockNode.get("type").asText();
            ssbo.blockSize = ssboBlockNode.get("block_size").asInt();
            ssbos.add(ssbo);
        }

        ssbos.sort(new SortBindingsComparator());

        return ssbos;
    }

    private void addTexturesFromNode(JsonNode node, ArrayList<Resource> textures) {
        if (node != null) {
            for (Iterator<JsonNode> iter = node.getElements(); iter.hasNext();) {
                JsonNode textureNode = iter.next();
                Resource res     = new Resource();
                res.name         = textureNode.get("name").asText();
                res.type         = textureNode.get("type").asText();
                res.binding      = textureNode.get("binding").asInt();
                res.set          = textureNode.get("set").asInt();
                res.blockSize    = 0;
                textures.add(res);
            }
        }
    }

    public ArrayList<Resource> getTextures() {
        ArrayList<Resource> textures = new ArrayList<>();
        addTexturesFromNode(root.get("textures"),          textures);
        addTexturesFromNode(root.get("separate_images"),   textures);
        addTexturesFromNode(root.get("images"),            textures);
        addTexturesFromNode(root.get("separate_samplers"), textures);

        textures.sort(new SortBindingsComparator());
        return textures;
    }

    public ArrayList<Resource> getInputs() {
        ArrayList<Resource> inputs = new ArrayList<>();

        JsonNode inputsNode = root.get("inputs");

        if (inputsNode == null) {
            return inputs;
        }

        for (Iterator<JsonNode> iter = inputsNode.getElements(); iter.hasNext();) {
            JsonNode inputNode = iter.next();
            Resource res = new Resource();
            res.name     = inputNode.get("name").asText();
            res.type     = inputNode.get("type").asText();
            res.binding  = inputNode.get("location").asInt();
            inputs.add(res);
        }

        inputs.sort(new SortBindingsComparator());

        return inputs;
    }

    public ArrayList<Resource> getOutputs() {
        ArrayList<Resource> outputs = new ArrayList<>();

        JsonNode outputsNode = root.get("outputs");

        if (outputsNode == null) {
            return outputs;
        }

        for (Iterator<JsonNode> iter = outputsNode.getElements(); iter.hasNext();) {
            JsonNode outputNode = iter.next();
            Resource res = new Resource();
            res.name     = outputNode.get("name").asText();
            res.type     = outputNode.get("type").asText();
            res.binding  = outputNode.get("location").asInt();
            outputs.add(res);
        }

        outputs.sort(new SortBindingsComparator());

        return outputs;
    }
}
