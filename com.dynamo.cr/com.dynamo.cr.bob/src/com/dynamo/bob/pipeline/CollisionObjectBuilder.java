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
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

@ProtoParams(srcClass = CollisionObjectDesc.class, messageClass = CollisionObjectDesc.class)
@BuilderParams(name="CollisionObject", inExts=".collisionobject", outExt=".collisionobjectc", paramsForSignature = {"physics-type-2D"})
public class CollisionObjectBuilder extends ProtoBuilder<CollisionObjectDesc.Builder> {

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
        }
        return taskBuilder.build();
    }

    private void compileMeshes(CollisionShape.Builder collisionShapeBuilder, IResource resource) throws IOException, CompileExceptionError {
        Map<String, Rig.MeshSet> meshSets = new HashMap<>();
        Map<String, IResource> sceneResources = new HashMap<>();

        for (int i = 0; i < collisionShapeBuilder.getShapesCount(); i++) {
            Shape.Builder shapeBuilder = collisionShapeBuilder.getShapesBuilder(i);
            if (!isSourceMeshShape(shapeBuilder)) {
                continue;
            }
            if (!shapeBuilder.hasMeshScene() || shapeBuilder.getMeshScene().isEmpty()) {
                continue;
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
                sceneResources.put(scenePath, sceneResource);
            }
        }

        try {
            CollisionMeshCompiler.compile(collisionShapeBuilder, meshSets);
        } catch (CollisionMeshCompiler.CompileException e) {
            IResource errorResource = e.getScenePath() == null ? resource : sceneResources.getOrDefault(e.getScenePath(), resource);
            throw new CompileExceptionError(errorResource, 0, e.getMessage());
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
