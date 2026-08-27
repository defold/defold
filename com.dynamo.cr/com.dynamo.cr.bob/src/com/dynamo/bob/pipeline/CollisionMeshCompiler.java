// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

package com.dynamo.bob.pipeline;

import com.dynamo.gamesys.proto.Physics.CollisionShape;
import com.dynamo.gamesys.proto.Physics.CollisionShape.Shape;
import com.dynamo.gamesys.proto.Physics.CollisionShape.Type;
import com.dynamo.rig.proto.Rig;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.HashMap;
import java.util.Map;

public final class CollisionMeshCompiler {

    public static final class CompileException extends Exception {
        private final String scenePath;

        CompileException(String scenePath, String message) {
            super(message);
            this.scenePath = scenePath;
        }

        public String getScenePath() {
            return scenePath;
        }
    }

    private static final class CompiledMesh {
        int dataIndex;
        int dataCount;
        int triangleIndex;
        int triangleCount;
    }

    private CollisionMeshCompiler() {
    }

    private static boolean isSourceMeshShape(Shape.Builder shape) {
        return shape.getShapeType() == Type.TYPE_MESH ||
                shape.getShapeType() == Type.TYPE_HULL &&
                (shape.hasMeshScene() || shape.hasMeshName() || shape.hasMeshIndex() || shape.getCount() == 0);
    }

    private static CompiledMesh compileMesh(Rig.Model model, String modelName, CollisionShape.Builder collisionShapeBuilder, String scenePath, boolean includeTriangles) throws CompileException {
        CompiledMesh compiledMesh = new CompiledMesh();
        compiledMesh.dataIndex = collisionShapeBuilder.getDataCount();
        compiledMesh.triangleIndex = collisionShapeBuilder.getIndicesCount() / 3;

        int vertexOffset = 0;
        for (Rig.Mesh mesh : model.getMeshesList()) {
            if (includeTriangles && mesh.getPrimitiveType() != Rig.PrimitiveType.PRIMITIVE_TYPE_TRIANGLES) {
                throw new CompileException(scenePath, String.format("Mesh '%s' contains a non-triangle primitive", modelName));
            }
            if (mesh.getPositionsCount() == 0 || mesh.getPositionsCount() % 3 != 0) {
                throw new CompileException(scenePath, String.format("Mesh '%s' contains invalid vertex data", modelName));
            }

            int indexSize = 0;
            int indexCount = 0;
            if (includeTriangles) {
                if (mesh.getIndicesFormat() == Rig.IndexBufferFormat.INDEXBUFFER_FORMAT_16) {
                    indexSize = Short.BYTES;
                } else if (mesh.getIndicesFormat() == Rig.IndexBufferFormat.INDEXBUFFER_FORMAT_32) {
                    indexSize = Integer.BYTES;
                } else {
                    throw new CompileException(scenePath, String.format("Mesh '%s' contains invalid triangle indices", modelName));
                }
                int indexByteCount = mesh.getIndices().size();
                if (indexByteCount == 0 || indexByteCount % indexSize != 0) {
                    throw new CompileException(scenePath, String.format("Mesh '%s' contains invalid triangle indices", modelName));
                }
                indexCount = indexByteCount / indexSize;
                if (indexCount % 3 != 0) {
                    throw new CompileException(scenePath, String.format("Mesh '%s' contains invalid triangle indices", modelName));
                }
            }

            int vertexCount = mesh.getPositionsCount() / 3;
            for (float position : mesh.getPositionsList()) {
                if (!Float.isFinite(position)) {
                    throw new CompileException(scenePath, String.format("Mesh '%s' contains a non-finite vertex position", modelName));
                }
                collisionShapeBuilder.addData(position);
            }
            if (includeTriangles) {
                ByteBuffer indices = mesh.getIndices().asReadOnlyByteBuffer().order(ByteOrder.LITTLE_ENDIAN);
                for (int i = 0; i < indexCount; ++i) {
                    int index = indexSize == Short.BYTES ? Short.toUnsignedInt(indices.getShort()) : indices.getInt();
                    if (index < 0 || index >= vertexCount) {
                        throw new CompileException(scenePath, String.format("Mesh '%s' contains an out-of-range triangle index", modelName));
                    }
                    collisionShapeBuilder.addIndices(vertexOffset + index);
                }
            }
            vertexOffset += vertexCount;
        }

        compiledMesh.dataCount = collisionShapeBuilder.getDataCount() - compiledMesh.dataIndex;
        compiledMesh.triangleCount = collisionShapeBuilder.getIndicesCount() / 3 - compiledMesh.triangleIndex;
        if (compiledMesh.dataCount == 0 || includeTriangles && compiledMesh.triangleCount == 0) {
            throw new CompileException(scenePath, String.format("Mesh '%s' contains no %s", modelName, includeTriangles ? "triangle geometry" : "vertices"));
        }
        return compiledMesh;
    }

    private static Rig.Model resolveGeometryModel(Rig.MeshSet meshSet, Rig.Model selectedModel) {
        if (selectedModel.getMeshesCount() > 0) {
            return selectedModel;
        }
        for (Rig.Model model : meshSet.getModelsList()) {
            if (model.getMeshIndex() == selectedModel.getMeshIndex()) {
                return model;
            }
        }
        return selectedModel;
    }

    public static void compile(CollisionShape.Builder collisionShapeBuilder, Map<String, Rig.MeshSet> meshSets) throws CompileException {
        Map<String, CompiledMesh> compiledMeshes = new HashMap<>();

        for (int i = 0; i < collisionShapeBuilder.getShapesCount(); i++) {
            Shape.Builder shapeBuilder = collisionShapeBuilder.getShapesBuilder(i);
            if (!isSourceMeshShape(shapeBuilder)) {
                continue;
            }

            boolean includeTriangles = shapeBuilder.getShapeType() == Type.TYPE_MESH;
            String shapeName = includeTriangles ? "Mesh" : "Hull";
            if (!shapeBuilder.hasMeshScene() || shapeBuilder.getMeshScene().isEmpty()) {
                throw new CompileException(null, shapeName + " collision shape has no scene");
            }
            if (!shapeBuilder.hasMeshName() || shapeBuilder.getMeshName().isEmpty()) {
                throw new CompileException(null, shapeName + " collision shape has no selected mesh");
            }
            String scenePath = shapeBuilder.getMeshScene();
            Rig.MeshSet meshSet = meshSets.get(scenePath);
            if (meshSet == null) {
                throw new CompileException(scenePath, String.format("Unable to resolve collision mesh scene '%s'", scenePath));
            }

            Rig.Model selectedModel;
            try {
                int meshIndex = shapeBuilder.hasMeshIndex() ? shapeBuilder.getMeshIndex() : -1;
                selectedModel = ModelUtil.resolveNamedMesh(meshSet, shapeBuilder.getMeshName(), meshIndex);
            } catch (IllegalArgumentException e) {
                throw new CompileException(scenePath, e.getMessage());
            }
            if (meshSet.getSplitModelIndicesList().contains(selectedModel.getMeshIndex())) {
                throw new CompileException(scenePath, String.format(
                        "Mesh '%s' was split because it exceeds the vertex limit and cannot be used for collision geometry",
                        shapeBuilder.getMeshName()));
            }

            String compiledMeshKey = shapeBuilder.getShapeType() + ":" + scenePath + "#" + selectedModel.getMeshIndex();
            CompiledMesh compiledMesh = compiledMeshes.get(compiledMeshKey);
            if (compiledMesh == null) {
                Rig.Model geometryModel = resolveGeometryModel(meshSet, selectedModel);
                compiledMesh = compileMesh(geometryModel, shapeBuilder.getMeshName(), collisionShapeBuilder, scenePath, includeTriangles);
                compiledMeshes.put(compiledMeshKey, compiledMesh);
            }

            shapeBuilder.setIndex(compiledMesh.dataIndex);
            shapeBuilder.setCount(compiledMesh.dataCount);
            if (includeTriangles) {
                shapeBuilder.setTriangleIndex(compiledMesh.triangleIndex);
                shapeBuilder.setTriangleCount(compiledMesh.triangleCount);
            } else {
                shapeBuilder.clearTriangleIndex();
                shapeBuilder.clearTriangleCount();
            }
            shapeBuilder.clearMeshScene();
            shapeBuilder.clearMeshName();
            shapeBuilder.clearMeshIndex();
        }
    }
}
