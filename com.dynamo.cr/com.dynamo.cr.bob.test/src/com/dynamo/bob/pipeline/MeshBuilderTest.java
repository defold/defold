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

import static org.junit.Assert.assertEquals;

import java.util.Collections;
import java.util.List;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Progress;
import com.dynamo.bob.Task;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.fs.IResource;
import com.dynamo.gamesys.proto.BufferProto.BufferDesc;
import com.dynamo.gamesys.proto.MeshProto.MeshDesc;

public class MeshBuilderTest extends AbstractProtoBuilderTest {

    private BufferDesc generatedVertexBuffer;
    private BufferDesc generatedIndexBuffer;
    private int meshTaskOutputCount;

    private static final String VERTICES = """
            [{"name":"position", "type":"float32", "count":3,
              "data":[0,0,0, 1,0,0, 1,1,0, 0,1,0]}]
            """;

    @Before
    public void setup() {
        addTestFiles();
        addFile("/mesh.material", "name: \"mesh\"\nvertex_program: \"/test.vp\"\nfragment_program: \"/test.fp\"\n");
        addFile("/vertices.buffer", VERTICES);
    }

    private MeshDesc buildMesh(String indexStream, String indexStreamName) throws Exception {
        addFile("/vertices.buffer", VERTICES.substring(0, VERTICES.lastIndexOf(']')) + "," + indexStream + "]");
        String mesh = "material: \"/mesh.material\"\nvertices: \"/vertices.buffer\"\nindex_stream: \"" + indexStreamName + "\"\n";
        addFile("/test.mesh", mesh);
        getProject().setInputs(Collections.singletonList("/test.mesh"));
        List<TaskResult> results = getProject().build(Progress.discarding(), "build");
        MeshDesc meshDesc = null;
        for (TaskResult result : results) {
            if (!result.isOk()) {
                throw new CompileExceptionError(getProject().getResource("/test.mesh"), result.getLineNumber(), result.getMessage());
            }
            Task task = result.getTask();
            for (IResource output : task.getOutputs()) {
                if (output.getPath().endsWith(".meshc")) {
                    meshDesc = MeshDesc.parseFrom(output.getContent());
                    meshTaskOutputCount = task.getOutputs().size();
                } else if (output.getPath().endsWith("_generated_vertices.bufferc")) {
                    generatedVertexBuffer = BufferDesc.parseFrom(output.getContent());
                } else if (output.getPath().endsWith("_generated_indices.bufferc")) {
                    generatedIndexBuffer = BufferDesc.parseFrom(output.getContent());
                }
            }
        }
        return meshDesc;
    }

    @Test
    public void testIndexedMesh() throws Exception {
        MeshDesc mesh = buildMesh("{\"name\":\"index\", \"type\":\"uint16\", \"count\":1, \"data\":[0,1,2,0,2,3]}", "index");
        assertEquals("/test_generated_vertices.bufferc", mesh.getVertices());
        assertEquals("/test_generated_indices.bufferc", mesh.getIndices());
        assertEquals("index", mesh.getIndexStream());
        assertEquals(3, meshTaskOutputCount);

        assertEquals(1, generatedVertexBuffer.getStreamsCount());
        assertEquals("position", generatedVertexBuffer.getStreams(0).getName());
        assertEquals(1, generatedIndexBuffer.getStreamsCount());
        assertEquals("index", generatedIndexBuffer.getStreams(0).getName());
        assertEquals(List.of(0, 1, 2, 0, 2, 3), generatedIndexBuffer.getStreams(0).getUiList());
    }

    @Test
    public void testNonIndexedMeshUsesOriginalBufferResource() throws Exception {
        MeshDesc mesh = buildMesh("{\"name\":\"other\", \"type\":\"float32\", \"count\":1, \"data\":[0,0,0,0]}", "");
        assertEquals("/vertices.bufferc", mesh.getVertices());
        assertEquals("", mesh.getIndices());
        assertEquals(1, meshTaskOutputCount);
    }

    @Test(expected = CompileExceptionError.class)
    public void testIndexStreamMustExist() throws Exception {
        buildMesh("{\"name\":\"index\", \"type\":\"uint16\", \"count\":1, \"data\":[0]}", "missing");
    }

    @Test(expected = CompileExceptionError.class)
    public void testIndexBufferMustBeScalar() throws Exception {
        buildMesh("{\"name\":\"index\", \"type\":\"uint16\", \"count\":2, \"data\":[0,1]}", "index");
    }

    @Test(expected = CompileExceptionError.class)
    public void testIndexBufferMustBeUnsigned16Or32() throws Exception {
        buildMesh("{\"name\":\"index\", \"type\":\"int32\", \"count\":1, \"data\":[0]}", "index");
    }

    @Test(expected = CompileExceptionError.class)
    public void testIndexMustReferenceVertex() throws Exception {
        buildMesh("{\"name\":\"index\", \"type\":\"uint32\", \"count\":1, \"data\":[4]}", "index");
    }
}
