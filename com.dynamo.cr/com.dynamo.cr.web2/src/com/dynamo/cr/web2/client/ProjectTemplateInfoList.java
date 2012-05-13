package com.dynamo.cr.web2.client;

import com.google.gwt.core.client.JsArray;

public class ProjectTemplateInfoList extends BaseResponse {

    protected ProjectTemplateInfoList() {
    }

    public final native JsArray<ProjectTemplateInfo> getTemplates() /*-{
        return this.templates;
    }-*/;

}
