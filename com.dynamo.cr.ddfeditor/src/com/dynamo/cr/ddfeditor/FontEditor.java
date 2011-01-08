package com.dynamo.cr.ddfeditor;

import com.dynamo.render.proto.Font.FontDesc;


public class FontEditor extends DdfEditor {

    public FontEditor() {
        super(FontDesc.newBuilder().buildPartial());
    }
}
