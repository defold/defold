package com.dynamo.cr.web2.client;

import com.google.gwt.core.client.JsArray;

public class UserInfoList extends BaseResponse {

    protected UserInfoList() {
    }

    public final native JsArray<UserInfo> getUsers() /*-{
        return this.users;
    }-*/;

}
