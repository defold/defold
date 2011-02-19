package com.dynamo.cr.contenteditor.resource;

import com.dynamo.physics.proto.Physics.ConvexShape;

public class ConvexShapeResource {

    private ConvexShape convexShape;

    public ConvexShapeResource(ConvexShape convexShape) {
        this.convexShape = convexShape;
    }

    public ConvexShape getConvextShape() {
        return convexShape;
    }

}
