package com.dynamo.cr.ddfeditor;

import com.dynamo.render.proto.Font.FontDesc;
import com.google.protobuf.Message;


public class FontEditor extends DdfEditor {

    public FontEditor() {
        super(FontDesc.newBuilder().buildPartial());
    }

    public static Message newInitialWizardContent() {
        return FontDesc.newBuilder()
            .setFont("")
            .setMaterial("")
            .setSize(15)
            .build();
    }
}
