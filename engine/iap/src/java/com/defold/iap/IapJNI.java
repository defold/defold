package com.defold.iap;

public class IapJNI implements IListProductsListener, IPurchaseListener {

    // NOTE: Also defined in iap.h
    public static final int TRANS_STATE_PURCHASING = 0;
    public static final int TRANS_STATE_PURCHASED = 1;
    public static final int TRANS_STATE_FAILED = 2;
    public static final int TRANS_STATE_RESTORED = 3;
    public static final int TRANS_STATE_UNVERIFIED = 4;

    public static final int BILLING_RESPONSE_RESULT_OK = 0;
    public static final int BILLING_RESPONSE_RESULT_USER_CANCELED = 1;
    public static final int BILLING_RESPONSE_RESULT_SERVICE_UNAVAILABLE = 2;
    public static final int BILLING_RESPONSE_RESULT_BILLING_UNAVAILABLE = 3;
    public static final int BILLING_RESPONSE_RESULT_ITEM_UNAVAILABLE = 4;
    public static final int BILLING_RESPONSE_RESULT_DEVELOPER_ERROR = 5;
    public static final int BILLING_RESPONSE_RESULT_ERROR = 6;
    public static final int BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED = 7;
    public static final int BILLING_RESPONSE_RESULT_ITEM_NOT_OWNED = 8;

    public IapJNI() {
    }

    @Override
    public native void onProductsResult(int responseCode, String productList);

    @Override
    public native void onPurchaseResult(int responseCode, String purchaseData);

}
