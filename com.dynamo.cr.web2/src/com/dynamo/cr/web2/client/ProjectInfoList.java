package com.dynamo.cr.web2.client;

import com.google.gwt.core.client.JsArray;

public class ProjectInfoList extends BaseResponse {

    protected ProjectInfoList() {
    }

    public final native JsArray<ProjectInfo> getProjects() /*-{
        return this.projects;
    }-*/;

}
