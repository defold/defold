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
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
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
import com.dynamo.gamesys.proto.MeshProto.MeshDesc.IndexBufferFormat;
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

    private static int getStreamElementCount(StreamDesc stream) {
        return stream.getValueCount() > 0 ? getValueCount(stream) / stream.getValueCount() : 0;
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

    private static StreamDesc findStream(BufferDesc buffer, String name) {
        for (StreamDesc stream : buffer.getStreamsList()) {
            if (stream.getName().equals(name)) {
                return stream;
            }
        }
        return null;
    }

    private static int getVertexCount(BufferDesc vertices, String indexStreamName) {
        int vertexCount = 0;
        for (StreamDesc stream : vertices.getStreamsList()) {
            if (!stream.getName().equals(indexStreamName)) {
                vertexCount = Math.max(vertexCount, getStreamElementCount(stream));
            }
        }
        return vertexCount;
    }

    private static BufferDesc buildVertexBuffer(BufferDesc buffer, String indexStreamName) {
        BufferDesc.Builder vertexBuffer = BufferDesc.newBuilder();
        for (StreamDesc stream : buffer.getStreamsList()) {
            if (indexStreamName.isEmpty() || !stream.getName().equals(indexStreamName)) {
                vertexBuffer.addStreams(stream);
            }
        }
        return vertexBuffer.build();
    }

    private static byte[] validateAndPackIndices(IResource meshResource, BufferDesc vertices, String indexStreamName, MeshDesc.Builder meshDescBuilder) throws CompileExceptionError {
        StreamDesc stream = findStream(vertices, indexStreamName);
        if (stream == null) {
            throw new CompileExceptionError(meshResource, 0, "Index stream '" + indexStreamName + "' was not found in the vertex buffer.");
        }

        if (stream.getValueCount() != 1) {
            throw new CompileExceptionError(meshResource, 0, "Index stream must have count 1.");
        }

        ValueType valueType = stream.getValueType();
        if (valueType != ValueType.VALUE_TYPE_UINT16 && valueType != ValueType.VALUE_TYPE_UINT32) {
            throw new CompileExceptionError(meshResource, 0, "Index stream must use uint16 or uint32 values.");
        }

        int vertexCount = getVertexCount(vertices, indexStreamName);
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

        int indexSize = valueType == ValueType.VALUE_TYPE_UINT16 ? Short.BYTES : Integer.BYTES;
        ByteBuffer indexData = ByteBuffer.allocate(stream.getUiCount() * indexSize).order(ByteOrder.LITTLE_ENDIAN);
        for (int index : stream.getUiList()) {
            if (indexSize == Short.BYTES) {
                indexData.putShort((short) index);
            } else {
                indexData.putInt(index);
            }
        }

        meshDescBuilder.setIndexBufferFormat(valueType == ValueType.VALUE_TYPE_UINT16
                ? IndexBufferFormat.INDEXBUFFER_FORMAT_16
                : IndexBufferFormat.INDEXBUFFER_FORMAT_32);
        meshDescBuilder.setIndexCount(stream.getUiCount());
        meshDescBuilder.setVertexCount(vertexCount);
        return indexData.array();
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

        IResource verticesResource = findTaskInput(task, verticesInputPath);
        if (verticesResource == null) {
            throw new CompileExceptionError(resource, 0, "Unable to inspect mesh vertex buffer build input.");
        }
        BufferDesc vertices = BufferDesc.parseFrom(verticesResource.getContent());
        byte[] indexData = new byte[0];
        String indexStreamName = "";
        if (meshDescBuilder.hasIndexStream() && !meshDescBuilder.getIndexStream().isEmpty()) {
            indexStreamName = meshDescBuilder.getIndexStream();
            indexData = validateAndPackIndices(resource, vertices, indexStreamName, meshDescBuilder);
        } else {
            meshDescBuilder.setVertexCount(getElementCount(vertices));
        }

        byte[] vertexData = buildVertexBuffer(vertices, indexStreamName).toByteArray();
        meshDescBuilder.setVertexBufferSize(vertexData.length);

        List<String> newTextureList = new ArrayList<String>();
        for (String t : meshDescBuilder.getTexturesList()) {
            if (t.isEmpty())
                continue;
            BuilderUtil.checkResource(this.project, resource, "texture", t);
            newTextureList.add(ProtoBuilders.replaceTextureName(t));
        }
        meshDescBuilder.clearTextures();
        meshDescBuilder.addAllTextures(newTextureList);

        byte[] header = meshDescBuilder.build().toByteArray();
        ByteArrayOutputStream out = new ByteArrayOutputStream(Integer.BYTES + header.length + vertexData.length + indexData.length);
        out.write(ByteBuffer.allocate(Integer.BYTES).order(ByteOrder.LITTLE_ENDIAN).putInt(header.length).array());
        out.write(header);
        out.write(vertexData);
        out.write(indexData);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }
}
