package com.dynamo.cr.ddfeditor;

import com.dynamo.render.proto.Render.RenderPrototypeDesc;

public class RenderEditor extends DdfEditor {

    public RenderEditor() {
        super(RenderPrototypeDesc.newBuilder().buildPartial());
    }
}
