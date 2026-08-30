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

    private static int getStreamElementCount(StreamDesc stream) {
        return stream.getValueCount() > 0 ? getValueCount(stream) / stream.getValueCount() : 0;
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

    private static StreamDesc validateIndices(IResource meshResource, BufferDesc vertices, String indexStreamName) throws CompileExceptionError {
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

        return stream;
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        MeshDesc.Builder meshDescBuilder = getSrcBuilder(input);
        if (!meshDescBuilder.hasIndexStream() || meshDescBuilder.getIndexStream().isEmpty()) {
            return super.create(input);
        }

        IResource verticesResource = BuilderUtil.checkResource(project, input, "vertices", meshDescBuilder.getVertices());
        Task.TaskBuilder taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addInput(verticesResource)
                .addOutput(input.changeExt(params.outExt()))
                .addOutput(input.changeExt("_generated_vertices.bufferc"))
                .addOutput(input.changeExt("_generated_indices.bufferc"));

        createSubTask(meshDescBuilder.getMaterial(), "material", taskBuilder);
        for (String texture : meshDescBuilder.getTexturesList()) {
            if (!texture.isEmpty()) {
                createSubTask(texture, "textures", taskBuilder);
            }
        }
        return taskBuilder.build();
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        ByteArrayInputStream mesh_is = new ByteArrayInputStream(task.input(0).getContent());
        InputStreamReader mesh_isr = new InputStreamReader(mesh_is);
        MeshDesc.Builder meshDescBuilder = MeshDesc.newBuilder();
        TextFormat.merge(mesh_isr, meshDescBuilder);

        IResource resource = task.input(0);
        BuilderUtil.checkResource(this.project, resource, "vertices", meshDescBuilder.getVertices());
        String verticesPath = ResourceUtil.minifyPathAndReplaceExt(meshDescBuilder.getVertices(), ".buffer", ".bufferc");
        meshDescBuilder.setVertices(verticesPath);
        BuilderUtil.checkResource(this.project, resource, "material", meshDescBuilder.getMaterial());
        meshDescBuilder.setMaterial(ResourceUtil.minifyPathAndReplaceExt(meshDescBuilder.getMaterial(), ".material", ".materialc"));

        if (meshDescBuilder.hasIndexStream() && !meshDescBuilder.getIndexStream().isEmpty()) {
            BufferDesc sourceBuffer = BufferBuilder.parseBuffer(task.input(1));
            String indexStreamName = meshDescBuilder.getIndexStream();
            StreamDesc indexStream = validateIndices(resource, sourceBuffer, indexStreamName);
            BufferDesc vertexBuffer = buildVertexBuffer(sourceBuffer, indexStreamName);
            BufferDesc indexBuffer = BufferDesc.newBuilder().addStreams(indexStream).build();

            meshDescBuilder.setVertices(BuilderUtil.getRelativePath(project, task.output(1)));
            meshDescBuilder.setIndices(BuilderUtil.getRelativePath(project, task.output(2)));
            task.output(1).setContent(vertexBuffer.toByteArray());
            task.output(2).setContent(indexBuffer.toByteArray());
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

        ByteArrayOutputStream out = new ByteArrayOutputStream(4 * 1024);
        meshDescBuilder.build().writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }
}
