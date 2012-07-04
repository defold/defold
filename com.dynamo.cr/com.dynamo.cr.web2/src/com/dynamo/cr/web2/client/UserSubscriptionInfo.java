package com.dynamo.cr.web2.client;

public class UserSubscriptionInfo extends BaseResponse {
    // NOTE match with cr_protocol_ddf.proto
    public static final int STATE_CANCELED = -1;
    public static final int STATE_PENDING = 0;
    public static final int STATE_ACTIVE = 1;

    protected UserSubscriptionInfo() {

    }

    public final native String getState() /*-{
		return this.state;
    }-*/;

    public final native ProductInfo getProductInfo() /*-{
		return this.product;
    }-*/;

    public final native CreditCardInfo getCreditCardInfo() /*-{
		return this.credit_card;
    }-*/;

    public final native String getUpdateURL() /*-{
		return this.update_url;
    }-*/;

}


