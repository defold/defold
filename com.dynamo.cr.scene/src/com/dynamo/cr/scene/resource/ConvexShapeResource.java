package com.dynamo.cr.scene.resource;

import com.dynamo.physics.proto.Physics.ConvexShape;

public class ConvexShapeResource extends Resource {

    private ConvexShape convexShape;

    public ConvexShapeResource(String path, ConvexShape convexShape) {
        super(path);
        this.convexShape = convexShape;
    }

    public ConvexShape getConvexShape() {
        return convexShape;
    }

    public void setConvexShape(ConvexShape convexShape) {
        this.convexShape = convexShape;
    }

}
