package com.dynamo.cr.ddfeditor;

import com.dynamo.render.proto.Material.MaterialDesc;
import com.google.protobuf.Message;

public class MaterialEditor extends DdfEditor {

    public MaterialEditor() {
        super(MaterialDesc.newBuilder().buildPartial());
    }

    public static Message newInitialWizardContent() {
        return MaterialDesc.newBuilder()
            .setName("unnamed")
            .setVertexProgram("")
            .setFragmentProgram("")
            .build();
    }
}
