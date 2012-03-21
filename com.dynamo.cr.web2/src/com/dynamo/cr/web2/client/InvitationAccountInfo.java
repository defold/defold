package com.dynamo.cr.web2.client;

public class InvitationAccountInfo extends BaseResponse {
    protected InvitationAccountInfo() {

    }

    public final native int getOriginalCount() /*-{
        return this.original_count;
    }-*/;

    public final native int getCurrentCount() /*-{
        return this.current_count;
    }-*/;

}


