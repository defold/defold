package com.dynamo.cr.sceneed.core;

import javax.vecmath.Vector3d;

import com.dynamo.cr.sceneed.core.RenderContext.Pass;

public class RenderData<T extends Node> implements Comparable<RenderData<T>> {

    private Pass pass;
    private INodeRenderer<T> nodeRenderer;
    private T node;
    @SuppressWarnings("unused")
    private Vector3d position;
    private Object userData;
    private long key;

    private final static long DISTANCE_SHIFT = 0;
    private final static long PASS_SHIFT = DISTANCE_SHIFT + 32;

    public RenderData(Pass pass, INodeRenderer<T> nodeRenderer, T node,
            Vector3d position, Object userData) {
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

    public void calculateKey() {
        // TODO: Transform position with "this" transform when available
        // and calculate distance from eye
        long distance = 0;
        key = (distance << DISTANCE_SHIFT)
                | (((long) pass.ordinal()) << PASS_SHIFT);
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

}
