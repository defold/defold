package com.dynamo.cr.ddfeditor.scene;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.physics.proto.Physics.ConvexShape;

public class ConvexShapeNode extends Node {

    private final ConvexShape convexShape;

    public ConvexShapeNode(ConvexShape convexShape) {
        this.convexShape = convexShape;
    }

    public ConvexShape getConvexShape() {
        return convexShape;
    }

}
