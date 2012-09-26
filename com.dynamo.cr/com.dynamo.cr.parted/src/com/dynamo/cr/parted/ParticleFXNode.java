package com.dynamo.cr.parted;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.parted.curve.HermiteSpline;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.Node;

public class ParticleFXNode extends Node {

    private static final long serialVersionUID = 1L;

    @Property
    private ValueSpread test = new ValueSpread();

    public ParticleFXNode() {
        test.setCurve(new HermiteSpline());
    }

    public ParticleFXNode(Vector4d translation, Quat4d rotation) {
        super(translation, rotation);
    }

    public ValueSpread getTest() {
        return new ValueSpread(test);
    }

    public void setTest(ValueSpread test) {
        this.test.set(test);
    }

}
