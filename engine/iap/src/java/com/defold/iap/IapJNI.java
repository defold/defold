package com.defold.iap;

public class IapJNI implements IListProductsListener, IPurchaseListener {

    public IapJNI() {
    }

    @Override
    public native void onProductsResult(int responseCode, String productList);

    @Override
    public native void onPurchaseResult(int responseCode, String purchaseData);

}
