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

import java.io.IOException;
import java.util.List;

@ProtoParams(srcClass = CollisionObjectDesc.class, messageClass = CollisionObjectDesc.class)
@BuilderParams(name="CollisionObject", inExts=".collisionobject", outExt=".collisionobjectc", paramsForSignature = {"physics-type-2D"})
public class CollisionObjectBuilder extends ProtoBuilder<CollisionObjectDesc.Builder> {

    private void ValidateShapeTypes(List<CollisionShape.Shape> shapeList, IResource resource, boolean isPhysics2D) throws IOException, CompileExceptionError {
        for(Shape shape : shapeList) {
            if(shape.getShapeType() == Type.TYPE_CAPSULE) {
                if(isPhysics2D) {
                    throw new CompileExceptionError(resource, 0, BobNLS.bind(Messages.CollisionObjectBuilder_MISMATCHING_SHAPE_PHYSICS_TYPE, "Capsule", "2D" ));
                }
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
