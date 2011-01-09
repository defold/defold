package com.dynamo.cr.ddfeditor;

import com.dynamo.render.proto.Render.RenderPrototypeDesc;
import com.google.protobuf.Message;

public class RenderEditor extends DdfEditor {

    public RenderEditor() {
        super(RenderPrototypeDesc.newBuilder().buildPartial());
    }

    public static Message newInitialWizardContent() {
        return RenderPrototypeDesc.newBuilder()
            .setScript("")
            .build();
    }
}
