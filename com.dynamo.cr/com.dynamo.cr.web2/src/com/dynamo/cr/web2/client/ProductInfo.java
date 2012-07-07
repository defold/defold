package com.dynamo.cr.web2.client;


public class ProductInfo extends BaseResponse {
    protected ProductInfo() {
    }

    // TODO: This should be a long but not representable in javascript
    public final native int getId() /*-{
		return this.id;
    }-*/;

    public final native String getName() /*-{
		return this.name;
    }-*/;

    public final native int getMaxMemberCount() /*-{
		return this.max_member_count;
    }-*/;

    public final native int getFee() /*-{
		return this.fee;
    }-*/;

    public final native String getSignupURL() /*-{
		return this.signup_url;
    }-*/;

}
