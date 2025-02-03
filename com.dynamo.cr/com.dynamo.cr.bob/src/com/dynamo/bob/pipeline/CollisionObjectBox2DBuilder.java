package com.dynamo.bob.pipeline;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.ProtoParams;
import com.dynamo.gamesys.proto.Physics.CollisionObjectDesc;

@ProtoParams(srcClass = CollisionObjectDesc.class, messageClass = CollisionObjectDesc.class)
@BuilderParams(name="CollisionObjectBox2D", inExts=".collisionobject", outExt=".collisionobject_box2dc")
public class CollisionObjectBox2DBuilder extends CollisionObjectBuilder {

}
