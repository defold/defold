package com.dynamo.cr.web2.client;

public class DocumentationParameter extends BaseResponse {
    protected DocumentationParameter() {
    }

    public final native String getName() /*-{
        return this.name;
    }-*/;

    public final native String getDoc() /*-{
        return this.doc;
    }-*/;

}
