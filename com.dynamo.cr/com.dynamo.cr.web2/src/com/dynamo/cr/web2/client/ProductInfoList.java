package com.dynamo.cr.web2.client;

import com.google.gwt.core.client.JsArray;

public class ProductInfoList extends BaseResponse {

    protected ProductInfoList() {
    }

    public final native JsArray<ProductInfo> getProducts() /*-{
		return this.products;
    }-*/;

}
