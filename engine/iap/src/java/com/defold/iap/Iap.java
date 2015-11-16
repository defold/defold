package com.defold.iap;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;

import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.util.Log;

import com.android.vending.billing.IInAppBillingService;

public class Iap implements Handler.Callback {
    public static final String PARAM_PRODUCT = "product";
    public static final String PARAM_MESSENGER = "com.defold.iap.messenger";

    // NOTE: Also defined in iap_android.cpp
    public static final int TRANS_STATE_PURCHASING = 0;
    public static final int TRANS_STATE_PURCHASED = 1;
    public static final int TRANS_STATE_FAILED = 2;
    public static final int TRANS_STATE_RESTORED = 3;

    public static final String RESPONSE_CODE = "RESPONSE_CODE";
    public static final String RESPONSE_GET_SKU_DETAILS_LIST = "DETAILS_LIST";
    public static final String RESPONSE_BUY_INTENT = "BUY_INTENT";
    public static final String RESPONSE_INAPP_PURCHASE_DATA = "INAPP_PURCHASE_DATA";
    public static final String RESPONSE_INAPP_SIGNATURE = "INAPP_DATA_SIGNATURE";
    public static final String RESPONSE_INAPP_ITEM_LIST = "INAPP_PURCHASE_ITEM_LIST";
    public static final String RESPONSE_INAPP_PURCHASE_DATA_LIST = "INAPP_PURCHASE_DATA_LIST";
    public static final String RESPONSE_INAPP_SIGNATURE_LIST = "INAPP_DATA_SIGNATURE_LIST";
    public static final String INAPP_CONTINUATION_TOKEN = "INAPP_CONTINUATION_TOKEN";

    public static final int BILLING_RESPONSE_RESULT_OK = 0;
    public static final int BILLING_RESPONSE_RESULT_USER_CANCELED = 1;
    public static final int BILLING_RESPONSE_RESULT_SERVICE_UNAVAILABLE = 2;
    public static final int BILLING_RESPONSE_RESULT_BILLING_UNAVAILABLE = 3;
    public static final int BILLING_RESPONSE_RESULT_ITEM_UNAVAILABLE = 4;
    public static final int BILLING_RESPONSE_RESULT_DEVELOPER_ERROR = 5;
    public static final int BILLING_RESPONSE_RESULT_ERROR = 6;
    public static final int BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED = 7;
    public static final int BILLING_RESPONSE_RESULT_ITEM_NOT_OWNED = 8;

    public static enum Action {
        BUY,
        RESTORE,
        PROCESS_PENDING_CONSUMABLES
    }

    public static final String TAG = "iap";

    private Activity activity;
    private Handler handler;
    private Messenger messenger;
    private ServiceConnection serviceConn;
    private IInAppBillingService service;

    private SkuDetailsThread skuDetailsThread;
    private BlockingQueue<SkuRequest> skuRequestQueue = new ArrayBlockingQueue<SkuRequest>(16);

    private IPurchaseListener purchaseListener;
    private boolean initialized;

    private static class SkuRequest {
        private ArrayList<String> skuList;
        private IListProductsListener listener;

        public SkuRequest(ArrayList<String> skuList, IListProductsListener listener) {
            this.skuList = skuList;
            this.listener = listener;
        }
    }

    private class SkuDetailsThread extends Thread {
        public boolean stop = false;
        @Override
        public void run() {
            while (!stop) {
                try {
                    SkuRequest sr = skuRequestQueue.take();
                    if (service == null) {
                        Log.wtf(TAG,  "service is null");
                        sr.listener.onProductsResult(BILLING_RESPONSE_RESULT_ERROR, null);
                        continue;
                    }
                    if (activity == null) {
                        Log.wtf(TAG,  "activity is null");
                        sr.listener.onProductsResult(BILLING_RESPONSE_RESULT_ERROR, null);
                        continue;
                    }

                    try {
                        Bundle querySkus = new Bundle();
                        querySkus.putStringArrayList("ITEM_ID_LIST", sr.skuList);
                        Bundle skuDetails = service.getSkuDetails(3, activity.getPackageName(), "inapp", querySkus);
                        int response = skuDetails.getInt("RESPONSE_CODE");

                        String res = null;
                        if (response == BILLING_RESPONSE_RESULT_OK) {
                            ArrayList<String> responseList = skuDetails.getStringArrayList("DETAILS_LIST");

                            JSONObject map = new JSONObject();
                            for (String r : responseList) {
                                JSONObject product = convertProduct(r);
                                if (product != null) {
                                    map.put((String)product.get("ident"), product);
                                }
                            }
                            res = map.toString();
                        } else {
                            Log.e(TAG, "Failed to fetch product list: " + response);
                        }
                        sr.listener.onProductsResult(response, res);

                    } catch (RemoteException e) {
                        Log.e(TAG, "Failed to fetch product list", e);
                        sr.listener.onProductsResult(BILLING_RESPONSE_RESULT_ERROR, null);
                    } catch (JSONException e) {
                        Log.e(TAG, "Failed to fetch product list", e);
                        sr.listener.onProductsResult(BILLING_RESPONSE_RESULT_ERROR, null);
                    }
                } catch (InterruptedException e) {
                    continue;
                }
            }
        }
    }

    public Iap(Activity activity) {
        this.activity = activity;
    }

    private void init() {
        // NOTE: We must create Handler lazily as construction of
        // handlers must be in the context of a "looper" on Android

        if (this.initialized)
            return;

        this.initialized = true;
        this.handler = new Handler(this);
        this.messenger = new Messenger(this.handler);

        serviceConn = new ServiceConnection() {

            @Override
            public void onServiceDisconnected(ComponentName name) {
                Log.v(TAG, "IAP disconnected");
                service = null;
            }

            @Override
            public void onServiceConnected(ComponentName name, IBinder binderService) {
                Log.v(TAG, "IAP connected");
                service = IInAppBillingService.Stub.asInterface(binderService);
                skuDetailsThread = new SkuDetailsThread();
                skuDetailsThread.start();
            }
        };

        Intent serviceIntent = new Intent("com.android.vending.billing.InAppBillingService.BIND");
        // Limit intent to vending package
        serviceIntent.setPackage("com.android.vending");
        if (!activity.getPackageManager().queryIntentServices(serviceIntent, 0).isEmpty()) {
            // service available to handle that Intent
            activity.bindService(serviceIntent, serviceConn, Context.BIND_AUTO_CREATE);
        } else {
            serviceConn = null;
            Log.e(TAG, "Billing service unavailable on device.");
        }
    }

    public void stop() {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (serviceConn != null) {
                    activity.unbindService(serviceConn);
                    serviceConn = null;
                }
                if (skuDetailsThread != null) {
                    skuDetailsThread.stop = true;
                    skuDetailsThread.interrupt();
                    try {
                        skuDetailsThread.join();
                    } catch (InterruptedException e) {
                        Log.wtf(TAG, "Failed to join thread", e);
                    }
                }
            }
        });
    }

    public void listItems(final String skus, final IListProductsListener listener) {
        this.activity.runOnUiThread(new Runnable() {

            @Override
            public void run() {
                init();

                if (serviceConn != null) {
                    ArrayList<String> skuList = new ArrayList<String>();
                    for (String x : skus.split(",")) {
                        if (x.trim().length() > 0) {
                            skuList.add(x);
                        }
                    }

                    try {
                        skuRequestQueue.put(new SkuRequest(skuList, listener));
                    } catch (InterruptedException e) {
                        Log.wtf(TAG, "Failed to add sku request", e);
                    }
                } else {
                    listener.onProductsResult(BILLING_RESPONSE_RESULT_BILLING_UNAVAILABLE, null);
                }
            }
        });
    }

    private static JSONObject convertProduct(String purchase) {
        try {
            JSONObject p = new JSONObject(purchase);
            p.put("price_string", p.get("price"));
            p.put("ident", p.get("productId"));
            // It is not yet possible to obtain the price (num) and currency code on Android for the correct locale/region.
            // They have a currency code (price_currency_code), which reflects the merchant's locale, instead of the user's
            // https://code.google.com/p/marketbilling/issues/detail?id=93&q=currency%20code&colspec=ID%20Type%20Status%20Google%20Priority%20Milestone%20Owner%20Summary
            double price = 0.0;
            if (p.has("price_amount_micros")) {
                price = p.getLong("price_amount_micros") * 0.000001;
            }
            String currency_code = "Unknown";
            if (p.has("price_currency_code")) {
                currency_code = (String)p.get("price_currency_code");
            }
            p.put("currency_code", currency_code);
            p.put("price", price);

            p.remove("productId");
            p.remove("type");
            p.remove("price_amount_micros");
            p.remove("price_currency_code");
            return p;
        } catch (JSONException e) {
            Log.wtf(TAG, "Failed to convert product json", e);
        }

        return null;
    }

    public void buy(final String product, final IPurchaseListener listener) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                init();
                Iap.this.purchaseListener = listener;
                Intent intent = new Intent(activity, IapActivity.class);
                intent.putExtra(PARAM_MESSENGER, messenger);
                intent.putExtra(PARAM_PRODUCT, product);
                intent.setAction(Action.BUY.toString());
                activity.startActivity(intent);
            }
        });
    }

    public void processPendingConsumables(final IPurchaseListener listener) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                init();
                Iap.this.purchaseListener = listener;
                Intent intent = new Intent(activity, IapActivity.class);
                intent.putExtra(PARAM_MESSENGER, messenger);
                intent.setAction(Action.PROCESS_PENDING_CONSUMABLES.toString());
                activity.startActivity(intent);
            }
        });
    }

    public void restore(final IPurchaseListener listener) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                init();
                Iap.this.purchaseListener = listener;
                Intent intent = new Intent(activity, IapActivity.class);
                intent.putExtra(PARAM_MESSENGER, messenger);
                intent.setAction(Action.RESTORE.toString());
                activity.startActivity(intent);
            }
        });
    }

    public static String toISO8601(final Date date) {
        String formatted = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ssZ").format(date);
        return formatted.substring(0, 22) + ":" + formatted.substring(22);
    }

    private static String convertPurchase(String purchase, String signature) {
        try {
            JSONObject p = new JSONObject(purchase);
            p.put("ident", p.get("productId"));
            p.put("state", TRANS_STATE_PURCHASED);
            p.put("trans_ident", p.get("orderId"));
            p.put("date", toISO8601(new Date(p.getLong("purchaseTime"))));
            // Receipt is the complete json data
            // http://robertomurray.co.uk/blog/2013/server-side-google-play-in-app-billing-receipt-validation-and-testing/
            p.put("receipt", purchase);
            p.put("signature", signature);
            // TODO: How to simulate original_trans on iOS?

            p.remove("packageName");
            p.remove("orderId");
            p.remove("productId");
            p.remove("developerPayload");
            p.remove("purchaseTime");
            p.remove("purchaseState");
            p.remove("purchaseToken");

            return p.toString();

        } catch (JSONException e) {
            Log.wtf(TAG, "Failed to convert purchase json", e);
        }

        return null;
    }

    @Override
    public boolean handleMessage(Message msg) {
        Bundle bundle = msg.getData();

        String actionString = bundle.getString("action");
        if (actionString == null) {
            return false;
        }

        if (purchaseListener == null) {
            Log.wtf(TAG, "No purchase listener set");
            return false;
        }

        Action action = Action.valueOf(actionString);

        if (action == Action.BUY) {
            int responseCode = bundle.getInt(RESPONSE_CODE);
            String purchaseData = bundle.getString(RESPONSE_INAPP_PURCHASE_DATA);
            String dataSignature = bundle.getString(RESPONSE_INAPP_SIGNATURE);

            if (purchaseData != null && dataSignature != null) {
                purchaseData = convertPurchase(purchaseData, dataSignature);
            } else {
                purchaseData = "";
            }

            purchaseListener.onPurchaseResult(responseCode, purchaseData);
        } else if (action == Action.RESTORE) {
            Bundle items = bundle.getBundle("items");
            ArrayList<String> ownedSkus = items.getStringArrayList(RESPONSE_INAPP_ITEM_LIST);
            ArrayList<String> purchaseDataList = items.getStringArrayList(RESPONSE_INAPP_PURCHASE_DATA_LIST);
            ArrayList<String> signatureList = items.getStringArrayList(RESPONSE_INAPP_SIGNATURE_LIST);
            for (int i = 0; i < ownedSkus.size(); ++i) {
                int c = BILLING_RESPONSE_RESULT_OK;
                String pd = convertPurchase(purchaseDataList.get(i), signatureList.get(i));
                if (pd == null) {
                    pd = "";
                    c = BILLING_RESPONSE_RESULT_ERROR;
                }
                purchaseListener.onPurchaseResult(c, pd);
            }
        }
        return true;
    }
}
