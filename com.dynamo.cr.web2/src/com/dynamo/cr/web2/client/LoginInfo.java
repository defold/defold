package com.dynamo.cr.web2.client;

public class LoginInfo extends BaseResponse {
    protected LoginInfo() {

    }

    public final native int getUserId() /*-{
        return this.user_id;
    }-*/;

    public final native String getAuth() /*-{
        return this.auth;
    }-*/;
}


