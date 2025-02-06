package com.dynamo.bob.pipeline;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.BobNLS;
import com.dynamo.gamesys.proto.Physics;
import com.dynamo.gamesys.proto.Physics.CollisionObjectDesc;

import java.io.IOException;
import java.util.List;

@ProtoParams(srcClass = CollisionObjectDesc.class, messageClass = CollisionObjectDesc.class)
@BuilderParams(name="CollisionObjectBox2D", inExts=".collisionobject", outExt=".collisionobject_box2dc")
public class CollisionObjectBox2DBuilder extends CollisionObjectBuilder {

    @Override
    protected void ValidateShapeTypes(List<Physics.CollisionShape.Shape> shapeList, IResource resource, String physicsTypeStr) throws IOException, CompileExceptionError {
        for(Physics.CollisionShape.Shape shape : shapeList) {
            if(shape.getShapeType() == Physics.CollisionShape.Type.TYPE_CAPSULE) {
                throw new CompileExceptionError(resource, 0, BobNLS.bind(Messages.CollisionObjectBuilder_MISMATCHING_SHAPE_PHYSICS_TYPE, "Capsule", physicsTypeStr ));
            }
        }
    }

    @Override
    protected CollisionObjectDesc.Builder transform(Task task, IResource resource, CollisionObjectDesc.Builder messageBuilder) throws IOException, CompileExceptionError {
        CollisionObjectDesc.Builder builder = super.transform(task, resource, messageBuilder);
        builder.setCollisionShape(BuilderUtil.replaceExt(messageBuilder.getCollisionShape(), ".convexshape", ".convexshape_box2dc"));
        return builder;
    }
}
