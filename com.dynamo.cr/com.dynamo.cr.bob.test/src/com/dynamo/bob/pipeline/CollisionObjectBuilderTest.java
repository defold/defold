// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.gamesys.proto.Physics.CollisionObjectDesc;
import com.dynamo.gamesys.proto.Physics.CollisionShape;
import com.google.protobuf.Message;

import org.junit.Before;
import org.junit.Test;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.Base64;
import java.util.List;

public class CollisionObjectBuilderTest extends AbstractProtoBuilderTest {

    @Before
    public void use3DPhysics() {
        getProject().getProjectProperties().putStringValue("physics", "type", "3D");
    }

    private static byte[] makeMeshBuffer() {
        ByteBuffer buffer = ByteBuffer.allocate(86).order(ByteOrder.LITTLE_ENDIAN);

        float[] positions = {
                0.0f, 0.0f, 0.0f,
                1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f,
        };
        for (float position : positions) {
            buffer.putFloat(position);
        }
        buffer.putShort((short) 0);
        buffer.putShort((short) 1);
        buffer.putShort((short) 2);
        buffer.position(44);

        float[] secondPositions = {
                1.0f, 0.0f, 0.0f,
                1.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 1.0f,
        };
        for (float position : secondPositions) {
            buffer.putFloat(position);
        }
        buffer.putShort((short) 0);
        buffer.putShort((short) 1);
        buffer.putShort((short) 2);
        return buffer.array();
    }

    private static String makeGltf(String bufferUri, boolean duplicateName) {
        String secondMeshName = duplicateName ? "\"name\":\"Ground\"," : "\"name\":\"Other\",";
        return "{" +
                "\"asset\":{\"version\":\"2.0\"}," +
                "\"buffers\":[{\"uri\":\"" + bufferUri + "\",\"byteLength\":86}]," +
                "\"bufferViews\":[" +
                "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}," +
                "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}," +
                "{\"buffer\":0,\"byteOffset\":44,\"byteLength\":36}," +
                "{\"buffer\":0,\"byteOffset\":80,\"byteLength\":6}]," +
                "\"accessors\":[" +
                "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,0,1]}," +
                "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}," +
                "{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,0,1]}," +
                "{\"bufferView\":3,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}]," +
                "\"meshes\":[" +
                "{\"name\":\"Ground\",\"primitives\":[" +
                "{\"attributes\":{\"POSITION\":0},\"indices\":1}," +
                "{\"attributes\":{\"POSITION\":2},\"indices\":3}]}," +
                "{" + secondMeshName + "\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}," +
                "{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}]," +
                "\"nodes\":[{\"mesh\":0},{\"mesh\":1},{\"mesh\":2}]," +
                "\"scenes\":[{\"nodes\":[0,1,2]}],\"scene\":0}";
    }

    private static String embeddedBufferUri() {
        return "data:application/octet-stream;base64," + Base64.getEncoder().encodeToString(makeMeshBuffer());
    }

    private static byte[] makeGlb(boolean duplicateName) {
        String json = makeGltf("", duplicateName)
                .replace("\"uri\":\"\",\"byteLength\":86", "\"byteLength\":86");
        byte[] jsonBytes = json.getBytes(StandardCharsets.UTF_8);
        byte[] meshBuffer = makeMeshBuffer();
        int paddedJsonLength = (jsonBytes.length + 3) & ~3;
        int paddedBufferLength = (meshBuffer.length + 3) & ~3;
        int glbLength = 12 + 8 + paddedJsonLength + 8 + paddedBufferLength;
        ByteBuffer glb = ByteBuffer.allocate(glbLength).order(ByteOrder.LITTLE_ENDIAN);
        glb.putInt(0x46546c67);
        glb.putInt(2);
        glb.putInt(glbLength);
        glb.putInt(paddedJsonLength);
        glb.putInt(0x4e4f534a);
        glb.put(jsonBytes);
        while (glb.position() < 20 + paddedJsonLength) {
            glb.put((byte) ' ');
        }
        glb.putInt(paddedBufferLength);
        glb.putInt(0x004e4942);
        glb.put(meshBuffer);
        return glb.array();
    }

    private static String makeCollisionObject(String meshName, int meshIndex, int shapeCount) {
        return makeCollisionObject("TYPE_MESH", meshName, meshIndex, shapeCount);
    }

    private static String makeCollisionObject(String shapeType, String meshName, int meshIndex, int shapeCount) {
        StringBuilder shapes = new StringBuilder();
        for (int i = 0; i < shapeCount; ++i) {
            shapes.append("shapes {\n")
                    .append("  shape_type: ").append(shapeType).append("\n")
                    .append("  id: \"ground_").append(i).append("\"\n")
                    .append("  position { x: 0 y: 0 z: 0 }\n")
                    .append("  rotation { x: 0 y: 0 z: 0 w: 1 }\n")
                    .append("  index: 0\n")
                    .append("  count: 0\n")
                    .append("  mesh_scene: \"/mesh.gltf\"\n")
                    .append("  mesh_name: \"").append(meshName).append("\"\n")
                    .append("  mesh_index: ").append(meshIndex).append("\n")
                    .append("}\n");
        }
        return "type: COLLISION_OBJECT_TYPE_STATIC\n" +
                "mass: 0\n" +
                "friction: 0.5\n" +
                "restitution: 0\n" +
                "group: \"default\"\n" +
                "embedded_collision_shape {\n" + shapes + "}\n";
    }

    private CollisionObjectDesc buildCollisionObject(String source) throws Exception {
        List<Message> outputs = build("/mesh.collisionobject", source);
        return getMessage(outputs, CollisionObjectDesc.class);
    }

    private static int countInputs(Task task, String path) {
        int count = 0;
        for (IResource input : task.getInputs()) {
            if (path.equals(input.getPath())) {
                ++count;
            }
        }
        return count;
    }

    @Test
    public void testCompilesMeshGeometryAndSharesRanges() throws Exception {
        addFile("/mesh.gltf", makeGltf(embeddedBufferUri(), true));
        CollisionObjectDesc collisionObject = buildCollisionObject(makeCollisionObject("Ground", 0, 2));
        CollisionShape collisionShape = collisionObject.getEmbeddedCollisionShape();

        assertEquals(18, collisionShape.getDataCount());
        assertEquals(6, collisionShape.getIndicesCount());
        assertEquals(0, collisionShape.getIndices(0));
        assertEquals(1, collisionShape.getIndices(1));
        assertEquals(2, collisionShape.getIndices(2));
        assertEquals(3, collisionShape.getIndices(3));
        assertEquals(4, collisionShape.getIndices(4));
        assertEquals(5, collisionShape.getIndices(5));

        CollisionShape.Shape first = collisionShape.getShapes(0);
        CollisionShape.Shape second = collisionShape.getShapes(1);
        assertEquals(0, first.getIndex());
        assertEquals(18, first.getCount());
        assertEquals(0, first.getTriangleIndex());
        assertEquals(2, first.getTriangleCount());
        assertEquals(first.getIndex(), second.getIndex());
        assertEquals(first.getCount(), second.getCount());
        assertEquals(first.getTriangleIndex(), second.getTriangleIndex());
        assertEquals(first.getTriangleCount(), second.getTriangleCount());

        for (CollisionShape.Shape shape : collisionShape.getShapesList()) {
            assertFalse(shape.hasMeshScene());
            assertFalse(shape.hasMeshName());
            assertFalse(shape.hasMeshIndex());
        }
    }

    @Test
    public void testCompilesHullVerticesAndSharesRanges() throws Exception {
        addFile("/mesh.gltf", makeGltf(embeddedBufferUri(), true));
        CollisionObjectDesc collisionObject = buildCollisionObject(makeCollisionObject("TYPE_HULL", "Ground", 0, 2));
        CollisionShape collisionShape = collisionObject.getEmbeddedCollisionShape();

        assertEquals(18, collisionShape.getDataCount());
        assertEquals(0, collisionShape.getIndicesCount());

        CollisionShape.Shape first = collisionShape.getShapes(0);
        CollisionShape.Shape second = collisionShape.getShapes(1);
        assertEquals(CollisionShape.Type.TYPE_HULL, first.getShapeType());
        assertEquals(0, first.getIndex());
        assertEquals(18, first.getCount());
        assertFalse(first.hasTriangleIndex());
        assertFalse(first.hasTriangleCount());
        assertEquals(first.getIndex(), second.getIndex());
        assertEquals(first.getCount(), second.getCount());

        for (CollisionShape.Shape shape : collisionShape.getShapesList()) {
            assertFalse(shape.hasMeshScene());
            assertFalse(shape.hasMeshName());
            assertFalse(shape.hasMeshIndex());
        }
    }

    @Test
    public void testSourceHullIsRejectedFor2DPhysics() throws Exception {
        getProject().getProjectProperties().putStringValue("physics", "type", "2D");
        addFile("/mesh.gltf", makeGltf(embeddedBufferUri(), false));
        try {
            buildCollisionObject(makeCollisionObject("TYPE_HULL", "Ground", 0, 1));
            fail("Expected the Hull shape to be rejected for 2D physics");
        } catch (Exception e) {
            assertTrue(e.getMessage().contains("Hull"));
            assertTrue(e.getMessage().contains("2D"));
        }
    }

    @Test
    public void testMeshIsRejectedFor2DPhysics() throws Exception {
        getProject().getProjectProperties().putStringValue("physics", "type", "2D");
        addFile("/mesh.gltf", makeGltf(embeddedBufferUri(), false));
        try {
            buildCollisionObject(makeCollisionObject("Ground", 0, 1));
            fail("Expected the Mesh shape to be rejected for 2D physics");
        } catch (Exception e) {
            assertTrue(e.getMessage().contains("Mesh"));
            assertTrue(e.getMessage().contains("2D"));
        }
    }

    @Test
    public void testUniqueNameSurvivesStaleRawIndex() throws Exception {
        addFile("/mesh.gltf", makeGltf(embeddedBufferUri(), false));
        CollisionObjectDesc collisionObject = buildCollisionObject(makeCollisionObject("Ground", 2, 1));
        assertEquals(2, collisionObject.getEmbeddedCollisionShape().getShapes(0).getTriangleCount());
    }

    @Test
    public void testDuplicateNameRequiresMatchingRawIndex() throws Exception {
        addFile("/mesh.gltf", makeGltf(embeddedBufferUri(), true));
        try {
            buildCollisionObject(makeCollisionObject("Ground", 2, 1));
            fail("Expected the duplicate mesh selection to fail");
        } catch (Exception e) {
            assertTrue(e.getMessage().contains("does not exist at raw index 2"));
        }
    }

    @Test
    public void testUnnamedRawMeshCannotBeSelected() throws Exception {
        addFile("/mesh.gltf", makeGltf(embeddedBufferUri(), false));
        try {
            buildCollisionObject(makeCollisionObject("model2", 2, 1));
            fail("Expected the generated mesh name to be hidden from selection");
        } catch (Exception e) {
            assertTrue(e.getMessage().contains("was not found in the scene"));
        }
    }

    @Test
    public void testMissingNamedMeshIsRejected() throws Exception {
        addFile("/mesh.gltf", makeGltf(embeddedBufferUri(), false));
        try {
            buildCollisionObject(makeCollisionObject("Missing", 0, 1));
            fail("Expected a missing named mesh to be rejected");
        } catch (Exception e) {
            assertTrue(e.getMessage().contains("was not found in the scene"));
        }
    }

    @Test
    public void testNonTrianglePrimitiveIsRejected() throws Exception {
        String gltf = makeGltf(embeddedBufferUri(), false)
                .replace("\"indices\":1}", "\"indices\":1,\"mode\":1}");
        addFile("/mesh.gltf", gltf);
        try {
            buildCollisionObject(makeCollisionObject("Ground", 0, 1));
            fail("Expected a line primitive to be rejected");
        } catch (Exception e) {
            assertTrue(e.getMessage().contains("non-triangle primitive"));
        }
    }

    @Test
    public void testExternalMeshBuffer() throws Exception {
        addFile("/mesh.bin", makeMeshBuffer());
        addFile("/mesh.gltf", makeGltf("mesh.bin", false));
        CollisionObjectDesc collisionObject = buildCollisionObject(makeCollisionObject("Ground", 0, 1));
        assertEquals(18, collisionObject.getEmbeddedCollisionShape().getDataCount());
        assertEquals(6, collisionObject.getEmbeddedCollisionShape().getIndicesCount());
    }

    @Test
    public void testSceneAndExternalBufferAreDirectTaskInputs() throws Exception {
        addFile("/mesh.bin", makeMeshBuffer());
        addFile("/mesh.gltf", makeGltf("mesh.bin", false));
        addFile("/mesh.collisionobject", makeCollisionObject("Ground", 0, 2));

        Task task = getProject().createTask(getProject().getResource("/mesh.collisionobject"), CollisionObjectBuilder.class);

        assertEquals(1, countInputs(task, "mesh.gltf"));
        assertEquals(1, countInputs(task, "mesh.bin"));
    }

    @Test
    public void testBinaryGltfScene() throws Exception {
        addFile("/mesh.glb", makeGlb(false));
        String source = makeCollisionObject("Ground", 0, 1)
                .replace("/mesh.gltf", "/mesh.glb");
        CollisionObjectDesc collisionObject = buildCollisionObject(source);
        assertEquals(18, collisionObject.getEmbeddedCollisionShape().getDataCount());
        assertEquals(6, collisionObject.getEmbeddedCollisionShape().getIndicesCount());
    }
}
