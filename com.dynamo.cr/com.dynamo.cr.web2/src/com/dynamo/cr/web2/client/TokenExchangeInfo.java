package com.dynamo.cr.web2.client;

public class TokenExchangeInfo extends BaseResponse {
    protected TokenExchangeInfo() {

    }

    public final native String getFirstName() /*-{
        return this.first_name;
    }-*/;

    public final native String getLastName() /*-{
        return this.last_name;
    }-*/;

    public final native String getEmail() /*-{
        return this.email;
    }-*/;

    public final native int getUserId() /*-{
        return this.user_id;
    }-*/;

    public final native String getLoginToken() /*-{
        return this.login_token;
    }-*/;

    public final native String getType() /*-{
        return this.type;
    }-*/;

    public final native String getAuthToken() /*-{
        return this.auth_token;
    }-*/;

}


