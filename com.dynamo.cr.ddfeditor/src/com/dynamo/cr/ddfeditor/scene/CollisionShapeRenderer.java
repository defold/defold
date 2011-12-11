package com.dynamo.cr.ddfeditor.scene;
import java.util.EnumSet;

import com.dynamo.cr.sceneed.core.RenderContext.Pass;

public class CollisionShapeRenderer {

    protected static final float COLOR[] = new float[] { 255.0f / 255.0f, 247.0f / 255.0f, 73.0f/255.0f, 0.4f };
    protected static final EnumSet<Pass> passes = EnumSet.of(Pass.OUTLINE, Pass.TRANSPARENT, Pass.SELECTION);

    public CollisionShapeRenderer() {
        super();
    }

}