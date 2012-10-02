package com.dynamo.cr.parted.curve;

public class SplinePoint {

    private double x;
    private double y;
    private double tx;
    private double ty;

    public SplinePoint(double x, double y, double tx, double ty) {
        this.x = x;
        this.y = y;

        tx = Math.max(tx, 0);
        double l = Math.sqrt((tx * tx + ty * ty));
        this.tx = tx / l;
        this.ty = ty / l;
    }

    public SplinePoint(SplinePoint p) {
        this.x = p.x;
        this.y = p.y;
        this.tx = p.tx;
        this.ty = p.ty;
    }

    SplinePoint setTangent_(double tx, double ty) {
        return new SplinePoint(x, y, tx, ty);
    }

    public double getX() {
        return x;
    }

    public void setX(double x) {
        this.x = x;
    }

    public double getY() {
        return y;
    }

    public void setY(double y) {
        this.y = y;
    }

    public double getTx() {
        return tx;
    }

    public void setTx(double tx) {
        this.tx = tx;
    }

    public double getTy() {
        return ty;
    }

    public void setTy(double ty) {
        this.ty = ty;
    }

    @Override
    public String toString() {
        return String.format("[%f, %f, %f, %f]", x, y, tx, ty);
    }


}
