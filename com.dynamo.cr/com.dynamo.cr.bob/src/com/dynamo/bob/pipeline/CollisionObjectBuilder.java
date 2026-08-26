package com.dynamo.bob.pipeline;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.bob.util.BobNLS;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.gamesys.proto.Physics.ConvexShape;
import com.dynamo.gamesys.proto.Physics.CollisionObjectDesc;
import com.dynamo.gamesys.proto.Physics.CollisionShape.Shape;
import com.dynamo.gamesys.proto.Physics.CollisionShape.Type;
import com.dynamo.gamesys.proto.Physics.CollisionShape;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.rig.proto.Rig;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

@ProtoParams(srcClass = CollisionObjectDesc.class, messageClass = CollisionObjectDesc.class)
@BuilderParams(name="CollisionObject", inExts=".collisionobject", outExt=".collisionobjectc", paramsForSignature = {"physics-type-2D"})
public class CollisionObjectBuilder extends ProtoBuilder<CollisionObjectDesc.Builder> {

    private static class CompiledMesh {
        int dataIndex;
        int dataCount;
        int triangleIndex;
        int triangleCount;
    }

    private static boolean isSourceMeshShape(Shape shape) {
        return shape.getShapeType() == Type.TYPE_MESH ||
                shape.getShapeType() == Type.TYPE_HULL &&
                (shape.hasMeshScene() || shape.hasMeshName() || shape.hasMeshIndex() || shape.getCount() == 0);
    }

    private static boolean isSourceMeshShape(Shape.Builder shape) {
        return shape.getShapeType() == Type.TYPE_MESH ||
                shape.getShapeType() == Type.TYPE_HULL &&
                (shape.hasMeshScene() || shape.hasMeshName() || shape.hasMeshIndex() || shape.getCount() == 0);
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        CollisionObjectDesc.Builder sourceBuilder = getSrcBuilder(input);
        Task.TaskBuilder taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        if (sourceBuilder.hasCollisionShape() && !sourceBuilder.getCollisionShape().isEmpty()) {
            createSubTask(sourceBuilder.getCollisionShape(), "collision_shape", taskBuilder);
        }

        Set<String> meshScenes = new HashSet<>();
        for (Shape shape : sourceBuilder.getEmbeddedCollisionShape().getShapesList()) {
            if (!isSourceMeshShape(shape) || !shape.hasMeshScene() || !meshScenes.add(shape.getMeshScene())) {
                continue;
            }
            IResource sceneResource = BuilderUtil.checkResource(project, input, "mesh_scene", shape.getMeshScene());
            Task meshsetTask = project.createTask(sceneResource);
            if (meshsetTask == null) {
                throw new CompileExceptionError(sceneResource, 0,
                        String.format("Unsupported resource type for 'mesh_scene': '%s'", sceneResource.getPath()));
            }
            taskBuilder.addInput(meshsetTask.output(0));
            if (project.option("model-split-large-meshes", "false").equals("true")) {
                taskBuilder.addInput(meshsetTask.output(3));
            }
        }
        return taskBuilder.build();
    }

    private static CompiledMesh compileMesh(Rig.Model model, String modelName, CollisionShape.Builder collisionShapeBuilder, IResource resource, boolean includeTriangles) throws CompileExceptionError {
        CompiledMesh compiledMesh = new CompiledMesh();
        compiledMesh.dataIndex = collisionShapeBuilder.getDataCount();
        compiledMesh.triangleIndex = collisionShapeBuilder.getIndicesCount() / 3;

        int vertexOffset = 0;
        for (Rig.Mesh mesh : model.getMeshesList()) {
            if (includeTriangles && mesh.getPrimitiveType() != Rig.PrimitiveType.PRIMITIVE_TYPE_TRIANGLES) {
                throw new CompileExceptionError(resource, 0, String.format("Mesh '%s' contains a non-triangle primitive", modelName));
            }
            if (mesh.getPositionsCount() == 0 || mesh.getPositionsCount() % 3 != 0) {
                throw new CompileExceptionError(resource, 0, String.format("Mesh '%s' contains invalid vertex data", modelName));
            }

            int indexSize = 0;
            int indexCount = 0;
            if (includeTriangles) {
                if (mesh.getIndicesFormat() == Rig.IndexBufferFormat.INDEXBUFFER_FORMAT_16) {
                    indexSize = Short.BYTES;
                } else if (mesh.getIndicesFormat() == Rig.IndexBufferFormat.INDEXBUFFER_FORMAT_32) {
                    indexSize = Integer.BYTES;
                } else {
                    throw new CompileExceptionError(resource, 0, String.format("Mesh '%s' contains invalid triangle indices", modelName));
                }
                int indexByteCount = mesh.getIndices().size();
                if (indexByteCount == 0 || indexByteCount % indexSize != 0) {
                    throw new CompileExceptionError(resource, 0, String.format("Mesh '%s' contains invalid triangle indices", modelName));
                }
                indexCount = indexByteCount / indexSize;
                if (indexCount % 3 != 0) {
                    throw new CompileExceptionError(resource, 0, String.format("Mesh '%s' contains invalid triangle indices", modelName));
                }
            }

            int vertexCount = mesh.getPositionsCount() / 3;
            for (float position : mesh.getPositionsList()) {
                if (!Float.isFinite(position)) {
                    throw new CompileExceptionError(resource, 0, String.format("Mesh '%s' contains a non-finite vertex position", modelName));
                }
                collisionShapeBuilder.addData(position);
            }
            if (includeTriangles) {
                ByteBuffer indices = mesh.getIndices().asReadOnlyByteBuffer().order(ByteOrder.LITTLE_ENDIAN);
                for (int i = 0; i < indexCount; ++i) {
                    int index = indexSize == Short.BYTES ? Short.toUnsignedInt(indices.getShort()) : indices.getInt();
                    if (index < 0 || index >= vertexCount) {
                        throw new CompileExceptionError(resource, 0, String.format("Mesh '%s' contains an out-of-range triangle index", modelName));
                    }
                    collisionShapeBuilder.addIndices(vertexOffset + index);
                }
            }
            vertexOffset += vertexCount;
        }

        compiledMesh.dataCount = collisionShapeBuilder.getDataCount() - compiledMesh.dataIndex;
        compiledMesh.triangleCount = collisionShapeBuilder.getIndicesCount() / 3 - compiledMesh.triangleIndex;
        if (compiledMesh.dataCount == 0 || includeTriangles && compiledMesh.triangleCount == 0) {
            throw new CompileExceptionError(resource, 0, String.format("Mesh '%s' contains no %s", modelName, includeTriangles ? "triangle geometry" : "vertices"));
        }
        return compiledMesh;
    }

    private static Rig.Model resolveGeometryModel(Rig.MeshSet meshSet, Rig.UncompactedMeshSet uncompactedMeshSet,
                                                  Rig.Model selectedModel) {
        if (uncompactedMeshSet != null) {
            for (Rig.Model model : uncompactedMeshSet.getModelsList()) {
                if (model.getMeshIndex() == selectedModel.getMeshIndex()) {
                    return model;
                }
            }
        }
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

    private void compileMeshes(CollisionShape.Builder collisionShapeBuilder, IResource resource) throws IOException, CompileExceptionError {
        Map<String, Rig.MeshSet> meshSets = new HashMap<>();
        Map<String, Rig.UncompactedMeshSet> uncompactedMeshSets = new HashMap<>();
        Map<String, CompiledMesh> compiledMeshes = new HashMap<>();
        boolean splitMeshes = project.option("model-split-large-meshes", "false").equals("true");

        for (int i = 0; i < collisionShapeBuilder.getShapesCount(); i++) {
            Shape.Builder shapeBuilder = collisionShapeBuilder.getShapesBuilder(i);
            if (!isSourceMeshShape(shapeBuilder)) {
                continue;
            }
            boolean includeTriangles = shapeBuilder.getShapeType() == Type.TYPE_MESH;
            String shapeName = includeTriangles ? "Mesh" : "Hull";
            if (!shapeBuilder.hasMeshScene() || shapeBuilder.getMeshScene().isEmpty()) {
                throw new CompileExceptionError(resource, 0, shapeName + " collision shape has no scene");
            }
            if (!shapeBuilder.hasMeshName() || shapeBuilder.getMeshName().isEmpty()) {
                throw new CompileExceptionError(resource, 0, shapeName + " collision shape has no selected mesh");
            }
            if (!shapeBuilder.hasMeshIndex()) {
                throw new CompileExceptionError(resource, 0, shapeName + " collision shape has no raw mesh index");
            }

            String scenePath = shapeBuilder.getMeshScene();
            IResource sceneResource = BuilderUtil.checkResource(project, resource, "mesh_scene", scenePath);
            String suffix = BuilderUtil.getSuffix(sceneResource.getPath()).toLowerCase();
            if (!suffix.equals("gltf") && !suffix.equals("glb")) {
                throw new CompileExceptionError(resource, 0, String.format("Mesh collision scene '%s' must be a .gltf or .glb resource", scenePath));
            }

            Rig.MeshSet meshSet = meshSets.get(scenePath);
            if (meshSet == null) {
                try {
                    meshSet = Rig.MeshSet.parseFrom(sceneResource.changeExt(".meshsetc").getContent());
                } catch (IOException e) {
                    throw new CompileExceptionError(sceneResource, 0, e.getMessage(), e);
                }
                meshSets.put(scenePath, meshSet);
            }

            Rig.UncompactedMeshSet uncompactedMeshSet = null;
            if (splitMeshes) {
                uncompactedMeshSet = uncompactedMeshSets.get(scenePath);
                if (uncompactedMeshSet == null) {
                    try {
                        uncompactedMeshSet = Rig.UncompactedMeshSet.parseFrom(
                                sceneResource.changeExt(MeshsetBuilder.UNCOMPACTED_MESHSET_EXT).getContent());
                    } catch (IOException e) {
                        throw new CompileExceptionError(sceneResource, 0, e.getMessage(), e);
                    }
                    uncompactedMeshSets.put(scenePath, uncompactedMeshSet);
                }
            }

            Rig.Model selectedModel;
            try {
                selectedModel = ModelUtil.resolveNamedMesh(meshSet, shapeBuilder.getMeshName(), shapeBuilder.getMeshIndex());
            } catch (IllegalArgumentException e) {
                throw new CompileExceptionError(sceneResource, 0, e.getMessage());
            }
            String compiledMeshKey = shapeBuilder.getShapeType() + ":" + scenePath + "#" + selectedModel.getMeshIndex();
            CompiledMesh compiledMesh = compiledMeshes.get(compiledMeshKey);
            if (compiledMesh == null) {
                Rig.Model geometryModel = resolveGeometryModel(meshSet, uncompactedMeshSet, selectedModel);
                compiledMesh = compileMesh(geometryModel, shapeBuilder.getMeshName(), collisionShapeBuilder, sceneResource, includeTriangles);
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

    private void ValidateShapeTypes(List<CollisionShape.Shape> shapeList, IResource resource, boolean isPhysics2D) throws IOException, CompileExceptionError {
        for(Shape shape : shapeList) {
            if(isPhysics2D && shape.getShapeType() == Type.TYPE_CAPSULE) {
                throw new CompileExceptionError(resource, 0, BobNLS.bind(Messages.CollisionObjectBuilder_MISMATCHING_SHAPE_PHYSICS_TYPE, "Capsule", "2D" ));
            }
            if(isPhysics2D && shape.getShapeType() == Type.TYPE_MESH) {
                throw new CompileExceptionError(resource, 0, BobNLS.bind(Messages.CollisionObjectBuilder_MISMATCHING_SHAPE_PHYSICS_TYPE, "Mesh", "2D" ));
            }
            if(isPhysics2D && shape.getShapeType() == Type.TYPE_HULL && isSourceMeshShape(shape)) {
                throw new CompileExceptionError(resource, 0, BobNLS.bind(Messages.CollisionObjectBuilder_MISMATCHING_SHAPE_PHYSICS_TYPE, "Hull", "2D" ));
            }
        }
    }

    @Override
    protected CollisionObjectDesc.Builder transform(Task task, IResource resource, CollisionObjectDesc.Builder messageBuilder) throws IOException, CompileExceptionError {
        if (messageBuilder.getEmbeddedCollisionShape().getShapesCount() == 0) {
            BuilderUtil.checkResource(this.project, resource, "collision shape", messageBuilder.getCollisionShape());
        }

        boolean isPhysics2D = this.project.option("physics-type-2D", "true").equals("true");

        // Merge convex shape resource with collision object
        // NOTE: Special case for tilegrid resources. They are left as is
        if(messageBuilder.hasEmbeddedCollisionShape()) {
            ValidateShapeTypes(messageBuilder.getEmbeddedCollisionShape().getShapesList(), resource, isPhysics2D);
        }
        if (messageBuilder.hasCollisionShape() && !messageBuilder.getCollisionShape().isEmpty() && !(messageBuilder.getCollisionShape().endsWith(".tilegrid") || messageBuilder.getCollisionShape().endsWith(".tilemap"))) {
            IResource shapeResource = project.getResource(messageBuilder.getCollisionShape().substring(1));
            ConvexShape.Builder cb = ConvexShape.newBuilder();
            ProtoUtil.merge(shapeResource, cb);
            CollisionShape.Builder eb = CollisionShape.newBuilder().mergeFrom(messageBuilder.getEmbeddedCollisionShape());
            ValidateShapeTypes(eb.getShapesList(), shapeResource, isPhysics2D);
            Shape.Builder sb = Shape.newBuilder()
                    .setShapeType(CollisionShape.Type.valueOf(cb.getShapeType().getNumber()))
                    .setPosition(Point3.newBuilder())
                    .setRotation(Quat.newBuilder().setW(1))
                    .setIndex(eb.getDataCount())
                    .setCount(cb.getDataCount());
            eb.addShapes(sb);
            eb.addAllData(cb.getDataList());
            messageBuilder.setEmbeddedCollisionShape(eb);
            messageBuilder.setCollisionShape("");
        }

        CollisionShape.Builder embeddedShapesBuilder = messageBuilder.getEmbeddedCollisionShapeBuilder();

        compileMeshes(embeddedShapesBuilder, resource);

        for (int i=0; i < embeddedShapesBuilder.getShapesCount(); i++) {
            CollisionShape.Shape.Builder shapeBuilder = embeddedShapesBuilder.getShapesBuilder(i);
            shapeBuilder.setIdHash(MurmurHash.hash64(shapeBuilder.getId()));
        }

        String path = messageBuilder.getCollisionShape();
        path = ResourceUtil.replaceExt(path, ".convexshape", ".convexshapec");
        path = ResourceUtil.replaceExt(path, ".tilegrid", ".tilemapc");
        path = ResourceUtil.replaceExt(path, ".tilemap", ".tilemapc");
        messageBuilder.setCollisionShape(ResourceUtil.minifyPath(path));

        return messageBuilder;
    }
}
