package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;

import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.physics.proto.Physics.ConvexShape;

@SuppressWarnings("serial")
public class ConvexShapeNode extends Node {

    private ConvexShape convexShape;

    public ConvexShapeNode() {
        super();
    }

    public ConvexShapeNode(ConvexShape convexShape) {
        super();
        this.convexShape = convexShape;
        updateAABB();
    }

    public ConvexShape getConvexShape() {
        return convexShape;
    }

    private void updateAABB() {
        AABB aabb = new AABB();
        ConvexShape s = this.convexShape;
        switch (s.getShapeType()) {
        case TYPE_BOX:
            float wExt = s.getData(0);
            float hExt = s.getData(1);
            float dExt = s.getData(2);
            aabb.union(wExt, hExt, dExt);
            aabb.union(-wExt, -hExt, -dExt);
            break;
        case TYPE_SPHERE:
            float r = s.getData(0);
            aabb.union(r, r, r);
            aabb.union(-r, -r, -r);
            break;
        case TYPE_CAPSULE:
            r = s.getData(0);
            float h = s.getData(1);
            aabb.union(r, r + h * 0.5f, r);
            aabb.union(-r, -r - h * 0.5f, -r);
            break;
        case TYPE_HULL:
            int n = s.getDataCount()/3*3;
            for (int i = 0; i < n; i += 3) {
                aabb.union(s.getData(i), s.getData(i+1), s.getData(i+2));
            }
        }
        setAABB(aabb);
    }

    private void writeObject(ObjectOutputStream out) throws IOException {
        this.convexShape.writeTo(out);
    }

    private void readObject(ObjectInputStream in) throws IOException, ClassNotFoundException {
        this.convexShape = ConvexShape.parseFrom(in);
    }

}
