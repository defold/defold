package com.dynamo.cr.sceneed.core.test;


@SuppressWarnings("serial")
public class DummyScaleAlongZ extends DummyNode {

    private boolean scaleAlongZ;

    public DummyScaleAlongZ(boolean scaleAlongZ) {
        this.scaleAlongZ = scaleAlongZ;
    }

    @Override
    protected boolean scaleAlongZ() {
        return this.scaleAlongZ;
    }
}
