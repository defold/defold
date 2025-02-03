package com.dynamo.bob.pipeline;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.ProtoParams;
import com.dynamo.gamesys.proto.Physics;

@ProtoParams(srcClass = Physics.CollisionObjectDesc.class, messageClass = Physics.CollisionObjectDesc.class)
@BuilderParams(name="CollisionObjectBullet3D", inExts=".collisionobject", outExt=".collisionobject_bullet3dc")
public class CollisionObjectBullet3DBuilder extends CollisionObjectBuilder {

}
