package com.dynamo.cr.parted.curve.test;

import com.dynamo.cr.parted.curve.HermiteSpline;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.Node;

public class DummyNode extends Node {

    private static final long serialVersionUID = 1L;

    @Property
    ValueSpread val1 = new ValueSpread(new HermiteSpline());
    @Property
    ValueSpread val2 = new ValueSpread(new HermiteSpline());

    public DummyNode() {
        this.val1.setAnimated(true);
        this.val2.setAnimated(true);
        HermiteSpline spline = (HermiteSpline)this.val2.getCurve();
        spline = spline.setPosition(0, 0.0, 1.5);
        spline = spline.setPosition(1, 1.0, 2.5);
        this.val2.setCurve(spline);
    }

    public ValueSpread getVal1() {
        return new ValueSpread(this.val1);
    }

    public void setVal1(ValueSpread val1) {
        this.val1.set(val1);
    }

    public ValueSpread getVal2() {
        return new ValueSpread(this.val2);
    }

    public void setVal2(ValueSpread val2) {
        this.val1.set(val2);
    }
}
