package com.dynamo.cr.web2.client;


public class NewProjectInfo extends BaseResponse {
    protected NewProjectInfo() {
    }

    // TODO: This should be a long but not representable in javascript
    public final native String getName() /*-{
        return this.name;
    }-*/;

    public final native String getDescription() /*-{
        return this.description;
    }-*/;

    public final native String getTemplateId() /*-{
        return this.templateId;
    }-*/;

    public final native String getLastName() /*-{
        return this.last_name;
    }-*/;

}
