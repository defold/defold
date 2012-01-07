package com.dynamo.cr.web2.client;

import com.google.gwt.core.client.JsArray;

public class Log extends BaseResponse {
    protected Log() {

    }

    public final native JsArray<CommitDesc> getCommits() /*-{
        return this.commits;
    }-*/;

}


