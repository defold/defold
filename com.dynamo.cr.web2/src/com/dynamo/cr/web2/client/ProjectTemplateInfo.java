package com.dynamo.cr.web2.client;


public class ProjectTemplateInfo extends BaseResponse {
    protected ProjectTemplateInfo() {
    }

    public final native String getId() /*-{
        return this.id;
    }-*/;

    public final native String getDescription() /*-{
        return this.description;
    }-*/;
}
