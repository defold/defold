package com.dynamo.cr.sceneed.core;

import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.RenderContext.Pass;

public class RenderData<T extends Node> implements Comparable<RenderData<T>> {

    private Pass pass;
    private INodeRenderer<T> nodeRenderer;
    private T node;
    private Point3d position;
    private Object userData;
    private long key;
    // Index overrides distance if set
    private Integer index = null;

    /*
     * NOTE: Do *NOT* use bit 63 in order to avoid signed numbers!
     */
    private final static long DISTANCE_SHIFT = 0;
    private final static long PASS_SHIFT = DISTANCE_SHIFT + 32;
    private final static long MANIPULATOR_SHIFT = 62;

    public RenderData(Pass pass, INodeRenderer<T> nodeRenderer, T node,
            Point3d position, Object userData) {
        this.pass = pass;
        this.nodeRenderer = nodeRenderer;
        this.node = node;
        this.position = position;
        this.userData = userData;
    }

    public Pass getPass() {
        return pass;
    }

    public INodeRenderer<T> getNodeRenderer() {
        return nodeRenderer;
    }

    public T getNode() {
        return node;
    }

    public Object getUserData() {
        return userData;
    }

    public long getKey() {
        return key;
    }

    /**
     * Setting the index overrides the distance calculation in the render key.
     * @param index
     */
    public void setIndex(int index) {
        this.index = index;
    }

    public void calculateKey(Camera camera, Matrix4d m, Point3d p) {
        long distance = 0;
        if (this.index == null) {
            p.set(this.position);
            this.node.getWorldTransform(m);
            // world space
            m.transform(p);
            // device space
            p = camera.project(p.getX(), p.getY(), p.getZ());
            // transform z (0,1) => (1,0) for back-to-front
            double z = 1.0 - p.getZ();
            // scale z (1,0) to int-space
            distance = (long)(z * Integer.MAX_VALUE);
        } else {
            distance = ((long)this.index) & 0xFFFFFFFFL;
        }
        key = (((long) pass.ordinal()) << PASS_SHIFT) | (distance << DISTANCE_SHIFT);

        if (node instanceof Manipulator) {
            key |= 1l << MANIPULATOR_SHIFT;
        }
    }

    @Override
    public int compareTo(RenderData<T> other) {
        if (key == other.key)
            return 0;
        else if (this.key > other.key)
            return 1;
        else
            return -1;
    }

    @Override
    public String toString() {
        return String.format("RenderData(%s, %d)", this.node, this.key);
    }

}
