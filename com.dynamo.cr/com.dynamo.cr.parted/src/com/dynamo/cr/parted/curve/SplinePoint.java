package com.dynamo.cr.parted.curve;

public class SplinePoint {

    public double x;
    public double y;
    public double tx;
    public double ty;

    public SplinePoint(double x, double y, double tx, double ty) {
        this.x = x;
        this.y = y;
        setTangent(tx, ty);
    }

    public SplinePoint(SplinePoint p) {
        this.x = p.x;
        this.y = p.y;
        this.tx = p.tx;
        this.ty = p.ty;
    }

    void setTangent(double tx, double ty) {
        tx = Math.max(tx, 0);
        double l = Math.sqrt((tx * tx + ty * ty));
        this.tx = tx / l;
        this.ty = ty / l;
    }

}
