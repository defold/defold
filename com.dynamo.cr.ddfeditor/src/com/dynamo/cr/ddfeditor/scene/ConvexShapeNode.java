package com.dynamo.cr.ddfeditor.scene;

import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.physics.proto.Physics.ConvexShape;

public class ConvexShapeNode extends Node {

    private ConvexShape convexShape;

    public ConvexShapeNode(ConvexShape convexShape) {
        this.convexShape = convexShape;
    }

    public ConvexShape getConvexShape() {
        return convexShape;
    }

    @Override
    protected Class<? extends NLS> getMessages() {
        return null;
    }

}
