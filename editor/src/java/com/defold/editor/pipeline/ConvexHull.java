package com.defold.editor.pipeline;

public class ConvexHull {
    String collisionGroup = "";
    int index = 0;
    int count = 0;

    public ConvexHull(ConvexHull convexHull) {
        this.collisionGroup = convexHull.collisionGroup;
        this.index = convexHull.index;
        this.count = convexHull.count;
    }

    public ConvexHull(String collisionGroup) {
        this.collisionGroup = collisionGroup;
    }

    public ConvexHull(String collisionGroup, int index, int count) {
        this.collisionGroup = collisionGroup;
        this.index = index;
        this.count = count;
    }

    public String getCollisionGroup() {
        return this.collisionGroup;
    }

    public void setCollisionGroup(String collisionGroup) {
        this.collisionGroup = collisionGroup;
    }

    public int getIndex() {
        return this.index;
    }

    public int getCount() {
        return this.count;
    }

    @Override
    public boolean equals(Object o) {
        if (o instanceof ConvexHull) {
            ConvexHull tile = (ConvexHull)o;
            return ((this.collisionGroup == tile.collisionGroup)
                    || (this.collisionGroup != null && this.collisionGroup.equals(tile.collisionGroup)))
                    && this.index == tile.index
                    && this.count == tile.count;
        } else {
            return super.equals(o);
        }
    }
}