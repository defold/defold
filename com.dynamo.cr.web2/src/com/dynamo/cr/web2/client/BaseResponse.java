package com.dynamo.cr.web2.client;

import com.google.gwt.core.client.JavaScriptObject;

// From: http://stackoverflow.com/questions/3449099/parse-json-with-gwt-2-0
public abstract class BaseResponse extends JavaScriptObject {

    protected BaseResponse() {

    }

    public static final native <T extends BaseResponse> T getResponse(String responseString) /*-{
        // TODO: Is eval() safe enough?
        // You should be able to use a safe parser here
        // (like the one from json.org)
        return eval('(' + responseString + ')');
    }-*/;
}
