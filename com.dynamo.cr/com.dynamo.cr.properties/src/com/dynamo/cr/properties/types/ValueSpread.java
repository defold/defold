package com.dynamo.cr.properties.types;



public class ValueSpread  {

    private double value;
    private double spread;
    private boolean animated;
    private Object curve;

    public ValueSpread() {
        super();
    }

    public ValueSpread(ValueSpread vs) {
        this.value = vs.value;
        this.spread = vs.spread;
        this.animated = vs.animated;
        this.curve = vs.curve;
    }

    public double getValue() {
        return value;
    }

    public void setValue(double value) {
        this.value = value;
    }

    public double getSpread() {
        return spread;
    }

    public void setSpread(double spread) {
        this.spread = spread;
    }

    public boolean isAnimated() {
        return animated;
    }

    public void setAnimated(boolean animated) {
        this.animated = animated;
    }

    @Override
    public boolean equals(Object other) {
        if (other instanceof ValueSpread) {
            ValueSpread vs = (ValueSpread) other;
            return value == vs.value
                && spread == vs.spread
                && animated == vs.animated
                && curve.equals(vs.curve);
        }
        return super.equals(other);
    }

    public void set(ValueSpread vs) {
        this.value = vs.value;
        this.spread = vs.spread;
        this.animated = vs.animated;
        this.curve = vs.curve;
    }

    public Object getCurve() {
        return curve;
    }

    public void setCurve(Object curve) {
        this.curve = curve;
    }

}
