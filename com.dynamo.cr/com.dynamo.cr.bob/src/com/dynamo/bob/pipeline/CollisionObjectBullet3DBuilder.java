package com.dynamo.bob.pipeline;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.gamesys.proto.Physics;

import java.io.IOException;

@ProtoParams(srcClass = Physics.CollisionObjectDesc.class, messageClass = Physics.CollisionObjectDesc.class)
@BuilderParams(name="CollisionObjectBullet3D", inExts=".collisionobject", outExt=".collisionobject_bullet3dc")
public class CollisionObjectBullet3DBuilder extends CollisionObjectBuilder {
    @Override
    protected Physics.CollisionObjectDesc.Builder transform(Task task, IResource resource, Physics.CollisionObjectDesc.Builder messageBuilder) throws IOException, CompileExceptionError {
        Physics.CollisionObjectDesc.Builder builder = super.transform(task, resource, messageBuilder);
        builder.setCollisionShape(BuilderUtil.replaceExt(messageBuilder.getCollisionShape(), ".convexshape", ".convexshape_bullet3dc"));
        return builder;
    }
}
