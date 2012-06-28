package com.dynamo.cr.web2.client;

public class CreditCardInfo extends BaseResponse {
    protected CreditCardInfo() {
    }

    public final native String getMaskedNumber() /*-{
		return this.masked_number;
    }-*/;

    public final native int getExpirationMonth() /*-{
		return this.expiration_month;
    }-*/;

    public final native int getExpirationYear() /*-{
		return this.expiration_year;
    }-*/;

}
