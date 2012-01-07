package com.dynamo.cr.web2.client;


public class CommitDesc extends BaseResponse {
    protected CommitDesc() {
    }

    public final native String getId() /*-{
        return this.id;
    }-*/;

    public final native String getMessage() /*-{
        return this.message;
    }-*/;

    public final native String getEmail() /*-{
        return this.email;
    }-*/;

    public final native String getName() /*-{
        return this.name;
    }-*/;

    public final native String getDate() /*-{
        return this.date;
    }-*/;

}
