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

package com.dynamo.bob.pipeline;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;

import com.dynamo.gamesys.proto.MeshProto.MeshDesc;
import com.dynamo.gamesys.proto.BufferProto.BufferDesc;
import com.dynamo.gamesys.proto.BufferProto.StreamDesc;
import com.dynamo.gamesys.proto.BufferProto.ValueType;
import com.google.protobuf.TextFormat;

@ProtoParams(srcClass = MeshDesc.class, messageClass = MeshDesc.class)
@BuilderParams(name="Mesh", inExts=".mesh", outExt=".meshc")
public class MeshBuilder extends ProtoBuilder<MeshDesc.Builder> {

    private static int getValueCount(StreamDesc stream) {
        switch (stream.getValueType()) {
            case VALUE_TYPE_UINT8:
            case VALUE_TYPE_UINT16:
            case VALUE_TYPE_UINT32:
                return stream.getUiCount();
            case VALUE_TYPE_UINT64:
                return stream.getUi64Count();
            case VALUE_TYPE_INT8:
            case VALUE_TYPE_INT16:
            case VALUE_TYPE_INT32:
                return stream.getICount();
            case VALUE_TYPE_INT64:
                return stream.getI64Count();
            case VALUE_TYPE_FLOAT32:
                return stream.getFCount();
            default:
                return 0;
        }
    }

    private static int getElementCount(BufferDesc buffer) {
        int elementCount = 0;
        for (StreamDesc stream : buffer.getStreamsList()) {
            if (stream.getValueCount() > 0) {
                elementCount = Math.max(elementCount, getValueCount(stream) / stream.getValueCount());
            }
        }
        return elementCount;
    }

    private static IResource findTaskInput(Task task, String path) {
        String resourcePath = Project.stripLeadingSlash(path);
        for (int i = 1; i < task.getInputs().size(); ++i) {
            IResource input = task.input(i);
            if (input.getPath().equals(resourcePath) || input.getPath().endsWith("/" + resourcePath)) {
                return input;
            }
        }
        return null;
    }

    private static void validateIndices(IResource meshResource, IResource verticesResource, IResource indicesResource) throws CompileExceptionError, IOException {
        BufferDesc vertices = BufferDesc.parseFrom(verticesResource.getContent());
        BufferDesc indices = BufferDesc.parseFrom(indicesResource.getContent());

        if (indices.getStreamsCount() != 1) {
            throw new CompileExceptionError(meshResource, 0, "Index buffer must contain exactly one stream.");
        }

        StreamDesc stream = indices.getStreams(0);
        if (stream.getValueCount() != 1) {
            throw new CompileExceptionError(meshResource, 0, "Index buffer stream must have count 1.");
        }

        ValueType valueType = stream.getValueType();
        if (valueType != ValueType.VALUE_TYPE_UINT16 && valueType != ValueType.VALUE_TYPE_UINT32) {
            throw new CompileExceptionError(meshResource, 0, "Index buffer stream must use uint16 or uint32 values.");
        }

        int vertexCount = getElementCount(vertices);
        long maxValue = valueType == ValueType.VALUE_TYPE_UINT16 ? 0xffffL : 0xffffffffL;
        for (int index : stream.getUiList()) {
            long unsignedIndex = Integer.toUnsignedLong(index);
            if (unsignedIndex > maxValue) {
                throw new CompileExceptionError(meshResource, 0, "Index value " + unsignedIndex + " does not fit the selected index type.");
            }
            if (unsignedIndex >= vertexCount) {
                throw new CompileExceptionError(meshResource, 0, "Index value " + unsignedIndex + " is outside the vertex buffer with " + vertexCount + " elements.");
            }
        }
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        ByteArrayInputStream mesh_is = new ByteArrayInputStream(task.input(0).getContent());
        InputStreamReader mesh_isr = new InputStreamReader(mesh_is);
        MeshDesc.Builder meshDescBuilder = MeshDesc.newBuilder();
        TextFormat.merge(mesh_isr, meshDescBuilder);

        IResource resource = task.input(0);
        BuilderUtil.checkResource(this.project, resource, "vertices", meshDescBuilder.getVertices());
        String verticesInputPath = ResourceUtil.replaceExt(meshDescBuilder.getVertices(), ".buffer", ".bufferc");
        String verticesPath = ResourceUtil.minifyPathAndReplaceExt(meshDescBuilder.getVertices(), ".buffer", ".bufferc");
        meshDescBuilder.setVertices(verticesPath);
        BuilderUtil.checkResource(this.project, resource, "material", meshDescBuilder.getMaterial());
        meshDescBuilder.setMaterial(ResourceUtil.minifyPathAndReplaceExt(meshDescBuilder.getMaterial(), ".material", ".materialc"));

        if (meshDescBuilder.hasIndices() && !meshDescBuilder.getIndices().isEmpty()) {
            BuilderUtil.checkResource(this.project, resource, "indices", meshDescBuilder.getIndices());
            String indicesInputPath = ResourceUtil.replaceExt(meshDescBuilder.getIndices(), ".buffer", ".bufferc");
            String indicesPath = ResourceUtil.minifyPathAndReplaceExt(meshDescBuilder.getIndices(), ".buffer", ".bufferc");
            meshDescBuilder.setIndices(indicesPath);

            IResource verticesResource = findTaskInput(task, verticesInputPath);
            IResource indicesResource = findTaskInput(task, indicesInputPath);
            if (verticesResource == null || indicesResource == null) {
                throw new CompileExceptionError(resource, 0, "Unable to validate mesh index buffer build inputs.");
            }
            validateIndices(resource, verticesResource, indicesResource);
        }

        List<String> newTextureList = new ArrayList<String>();
        for (String t : meshDescBuilder.getTexturesList()) {
            if (t.isEmpty())
                continue;
            BuilderUtil.checkResource(this.project, resource, "texture", t);
            newTextureList.add(ProtoBuilders.replaceTextureName(t));
        }
        meshDescBuilder.clearTextures();
        meshDescBuilder.addAllTextures(newTextureList);

        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        meshDescBuilder.build().writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }
}
