package com.dynamo.cr.properties.types;

import java.io.Serializable;

public class ValueSpread implements Serializable {

    private static final long serialVersionUID = 1L;
    private double value;
    private double spread;
    private boolean hideSpread;
    private boolean animated;
    private boolean curvable;
    private Object curve;

    public ValueSpread() {
        super();
        this.curvable = true;
    }

    public ValueSpread(Object curve) {
        this.curve = curve;
    }

    public ValueSpread(ValueSpread vs) {
        this.value = vs.value;
        this.spread = vs.spread;
        this.hideSpread = vs.hideSpread;
        this.animated = vs.animated;
        this.curvable = vs.curvable;
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

    public boolean hideSpread() {
        return this.hideSpread;
    }

    public void setHideSpread(boolean hideSpread) {
        this.hideSpread = hideSpread;
    }

    public boolean isAnimated() {
        return animated;
    }

    public void setAnimated(boolean animated) {
        this.animated = animated;
    }

    public boolean isCurvable() {
        return curvable;
    }

    public void setCurvable(boolean curvable) {
        this.curvable = curvable;
    }

    @Override
    public boolean equals(Object other) {
        if (other instanceof ValueSpread) {
            ValueSpread vs = (ValueSpread) other;
            return value == vs.value
                && spread == vs.spread
                && animated == vs.animated
                && curvable == vs.curvable
                && curve.equals(vs.curve);
        }
        return super.equals(other);
    }

    public void set(ValueSpread vs) {
        this.value = vs.value;
        this.spread = vs.spread;
        this.hideSpread = vs.hideSpread;
        this.animated = vs.animated;
        this.curvable = vs.curvable;
        this.curve = vs.curve;
    }

    public Object getCurve() {
        return curve;
    }

    public void setCurve(Object curve) {
        this.curve = curve;
    }

}
