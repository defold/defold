package com.dynamo.cr.web2.client;

import com.google.gwt.core.client.JsArray;


public class DocumentationElement extends BaseResponse {

    protected DocumentationElement() {
    }


    public final native String getType() /*-{
        return this.type;
    }-*/;

    public final native String getName() /*-{
        return this.name;
    }-*/;

    public final native String getBrief() /*-{
        return this.brief;
    }-*/;

    public final native String getDescription() /*-{
        return this.description;
    }-*/;

    public final native String getReturn() /*-{
        return this.return_;
    }-*/;

    public final native JsArray<DocumentationParameter> getParameters() /*-{
        return this.parameters;
    }-*/;

    public final native String getExamples() /*-{
        return this.examples;
    }-*/;

}
