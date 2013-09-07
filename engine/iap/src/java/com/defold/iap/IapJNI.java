package com.defold.iap;

public class IapJNI implements IListProductsListener, IPurchaseListener {

    public IapJNI() {
    }

    @Override
    public native void onResult(String productList);

    @Override
    public native void onResult(int responseCode, String purchaseData, String dataSignature);

}
