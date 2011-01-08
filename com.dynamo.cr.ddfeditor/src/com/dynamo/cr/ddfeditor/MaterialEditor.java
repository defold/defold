package com.dynamo.cr.ddfeditor;

import com.dynamo.render.proto.Material.MaterialDesc;

public class MaterialEditor extends DdfEditor {

    public MaterialEditor() {
        super(MaterialDesc.newBuilder().buildPartial());
    }
}
