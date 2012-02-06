package com.dynamo.cr.sceneed.ui;

import javax.vecmath.Point2i;

import com.dynamo.cr.sceneed.core.Node;

public class SelectionBoxNode extends Node {

    private boolean visible;
    private Point2i start;
    private Point2i current;

    public SelectionBoxNode() {
        this.visible = false;
        this.start = new Point2i();
        this.current = new Point2i();
    }

    public Point2i getStart() {
        return new Point2i(this.start);
    }

    public Point2i getCurrent() {
        return new Point2i(this.current);
    }

    public void set(int x, int y) {
        this.start.set(x, y);
        this.current.set(x, y);
    }

    public void setCurrent(int x, int y) {
        this.current.set(x, y);
    }

    public boolean isVisible() {
        return this.visible;
    }

    public void setVisible(boolean visible) {
        this.visible = visible;
    }
}
