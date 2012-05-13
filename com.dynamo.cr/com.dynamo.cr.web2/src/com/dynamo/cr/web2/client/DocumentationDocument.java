package com.dynamo.cr.web2.client;

import com.google.gwt.core.client.JsArray;

public class DocumentationDocument extends BaseResponse {
    protected DocumentationDocument() {
    }

    public final native JsArray<DocumentationElement> getElements() /*-{
        return this.elements;
    }-*/;

}
