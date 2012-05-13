package com.dynamo.cr.web2.client;


public class UserInfo extends BaseResponse {
    protected UserInfo() {
    }

    // TODO: This should be a long but not representable in javascript
    public final native int getId() /*-{
        return this.id;
    }-*/;

    public final native String getEmail() /*-{
        return this.email;
    }-*/;

    public final native String getFirstName() /*-{
        return this.first_name;
    }-*/;

    public final native String getLastName() /*-{
        return this.last_name;
    }-*/;

}
