package com.defold.iap;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;

import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.util.Log;

import com.android.vending.billing.IInAppBillingService;

public class IapGooglePlay implements Handler.Callback {
    public static final String PARAM_PRODUCT = "product";
    public static final String PARAM_PRODUCT_TYPE = "product_type";
    public static final String PARAM_PURCHASE_DATA = "purchase_data";
    public static final String PARAM_AUTOFINISH_TRANSACTIONS = "auto_finish_transactions";
    public static final String PARAM_MESSENGER = "com.defold.iap.messenger";

    public static final String RESPONSE_CODE = "RESPONSE_CODE";
    public static final String RESPONSE_GET_SKU_DETAILS_LIST = "DETAILS_LIST";
    public static final String RESPONSE_BUY_INTENT = "BUY_INTENT";
    public static final String RESPONSE_INAPP_PURCHASE_DATA = "INAPP_PURCHASE_DATA";
    public static final String RESPONSE_INAPP_SIGNATURE = "INAPP_DATA_SIGNATURE";
    public static final String RESPONSE_INAPP_ITEM_LIST = "INAPP_PURCHASE_ITEM_LIST";
    public static final String RESPONSE_INAPP_PURCHASE_DATA_LIST = "INAPP_PURCHASE_DATA_LIST";
    public static final String RESPONSE_INAPP_SIGNATURE_LIST = "INAPP_DATA_SIGNATURE_LIST";
    public static final String INAPP_CONTINUATION_TOKEN = "INAPP_CONTINUATION_TOKEN";

    public static enum Action {
        BUY,
        RESTORE,
        PROCESS_PENDING_CONSUMABLES,
        FINISH_TRANSACTION
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
    private boolean autoFinishTransactions;

    private static interface ISkuRequestListener {
    	public void onProducts(int resultCode, JSONObject products);
    }

    private static class SkuRequest {
        private ArrayList<String> skuList;
        private ISkuRequestListener listener;

        public SkuRequest(ArrayList<String> skuList, ISkuRequestListener listener) {
            this.skuList = skuList;
            this.listener = listener;
        }
    }

    private class SkuDetailsThread extends Thread {
        public boolean stop = false;

        private void addProductsFromBundle(Bundle skuDetails, JSONObject products) throws JSONException {
            int response = skuDetails.getInt("RESPONSE_CODE");
            if (response == IapJNI.BILLING_RESPONSE_RESULT_OK) {
                ArrayList<String> responseList = skuDetails.getStringArrayList("DETAILS_LIST");

                for (String r : responseList) {
                    JSONObject product = new JSONObject(r);
                    products.put(product.getString("productId"), product);
                }
            }
            else {
                Log.e(TAG, "Failed to fetch product list: " + response);
            }
        }

        @Override
        public void run() {
            while (!stop) {
                try {
                    SkuRequest sr = skuRequestQueue.take();
                    if (service == null) {
                        Log.wtf(TAG,  "service is null");
                        sr.listener.onProducts(IapJNI.BILLING_RESPONSE_RESULT_ERROR, null);
                        continue;
                    }
                    if (activity == null) {
                        Log.wtf(TAG,  "activity is null");
                        sr.listener.onProducts(IapJNI.BILLING_RESPONSE_RESULT_ERROR, null);
                        continue;
                    }

                    try {
                        Bundle querySkus = new Bundle();
                        querySkus.putStringArrayList("ITEM_ID_LIST", sr.skuList);

                        JSONObject products = new JSONObject();

                        Bundle inappSkuDetails = service.getSkuDetails(3, activity.getPackageName(), "inapp", querySkus);
                        addProductsFromBundle(inappSkuDetails, products);

                        Bundle subscriptionSkuDetails = service.getSkuDetails(3, activity.getPackageName(), "subs", querySkus);
                        addProductsFromBundle(subscriptionSkuDetails, products);

                        sr.listener.onProducts(IapJNI.BILLING_RESPONSE_RESULT_OK, products);

                    } catch (RemoteException e) {
                        Log.e(TAG, "Failed to fetch product list", e);
                        sr.listener.onProducts(IapJNI.BILLING_RESPONSE_RESULT_ERROR, null);
                    } catch (JSONException e) {
                        Log.e(TAG, "Failed to fetch product list", e);
                        sr.listener.onProducts(IapJNI.BILLING_RESPONSE_RESULT_ERROR, null);
                    }
                } catch (InterruptedException e) {
                    continue;
                }
            }
        }
    }

    public IapGooglePlay(Activity activity, boolean autoFinishTransactions) {
        this.activity = activity;
        this.autoFinishTransactions = autoFinishTransactions;
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
        List<ResolveInfo> intentServices = activity.getPackageManager().queryIntentServices(serviceIntent, 0);
        if (intentServices != null && !intentServices.isEmpty()) {
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


    private void queueSkuRequest(final SkuRequest request) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                init();

                if (serviceConn != null) {
                    try {
                        skuRequestQueue.put(request);
                    } catch (InterruptedException e) {
                        Log.wtf(TAG, "Failed to add sku request", e);
                        request.listener.onProducts(IapJNI.BILLING_RESPONSE_RESULT_BILLING_UNAVAILABLE, null);
                    }
                } else {
                    request.listener.onProducts(IapJNI.BILLING_RESPONSE_RESULT_BILLING_UNAVAILABLE, null);
                }
            }
        });
    }

    public void listItems(final String skus, final IListProductsListener listener) {
        ArrayList<String> skuList = new ArrayList<String>();
        for (String x : skus.split(",")) {
            if (x.trim().length() > 0) {
                skuList.add(x);
            }
        }

        queueSkuRequest(new SkuRequest(skuList, new ISkuRequestListener() {
            @Override
            public void onProducts(int resultCode, JSONObject products) {
                if (products != null && products.length() > 0) {
                    try {
                        // go through all of the products and convert them into
                        // the generic product format used for all IAP implementations
                        Iterator<String> keys = products.keys();
                        while(keys.hasNext()) {
                            String key = keys.next();
                            if (products.get(key) instanceof JSONObject ) {
                                JSONObject product = products.getJSONObject(key);
                                products.put(key, convertProduct(product));
                            }
                        }
                        listener.onProductsResult(resultCode, products.toString());
                    }
                    catch(JSONException e) {
                        Log.wtf(TAG, "Failed to convert products", e);
                        listener.onProductsResult(resultCode, null);
                    }
                }
                else {
                    listener.onProductsResult(resultCode, null);
                }
            }
        }));
    }

    // Convert the product data into the generic format shared between all Defold IAP implementations
    private static JSONObject convertProduct(JSONObject product) {
        try {
            // Deep copy and modify
            JSONObject p = new JSONObject(product.toString());
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

    private void buyProduct(final String product, final String type, final IPurchaseListener listener) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                init();
                IapGooglePlay.this.purchaseListener = listener;
                Intent intent = new Intent(activity, IapGooglePlayActivity.class);
                intent.putExtra(PARAM_MESSENGER, messenger);
                intent.putExtra(PARAM_AUTOFINISH_TRANSACTIONS, IapGooglePlay.this.autoFinishTransactions);
                intent.putExtra(PARAM_PRODUCT, product);
                intent.putExtra(PARAM_PRODUCT_TYPE, type);
                intent.setAction(Action.BUY.toString());
                activity.startActivity(intent);
            }
        });
    }

    public void buy(final String product, final IPurchaseListener listener) {
        ArrayList<String> skuList = new ArrayList<String>();
        skuList.add(product);
        queueSkuRequest(new SkuRequest(skuList, new ISkuRequestListener() {
            @Override
            public void onProducts(int resultCode, JSONObject products) {
                String type = "inapp";
                if (resultCode == IapJNI.BILLING_RESPONSE_RESULT_OK && products != null) {
                    try {
                        JSONObject productData = products.getJSONObject(product);
                        type = productData.getString("type");
                    }
                    catch(JSONException e) {
                        Log.wtf(TAG, "Failed to get product type before buying, assuming type 'inapp'", e);
                    }
                }
                else {
                    Log.wtf(TAG, "Failed to list product before buying, assuming type 'inapp'");
                }
                buyProduct(product, type, listener);
            }
        }));
    }

    public void finishTransaction(final String receipt, final IPurchaseListener listener) {
        if(IapGooglePlay.this.autoFinishTransactions) {
            return;
        }
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                init();
                IapGooglePlay.this.purchaseListener = listener;
                Intent intent = new Intent(activity, IapGooglePlayActivity.class);
                intent.putExtra(PARAM_MESSENGER, messenger);
                intent.putExtra(PARAM_AUTOFINISH_TRANSACTIONS, false);
                intent.putExtra(PARAM_PURCHASE_DATA, receipt);
                intent.setAction(Action.FINISH_TRANSACTION.toString());
                activity.startActivity(intent);
            }
        });
    }

    public void processPendingConsumables(final IPurchaseListener listener) {
        this.activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                init();
                IapGooglePlay.this.purchaseListener = listener;
                Intent intent = new Intent(activity, IapGooglePlayActivity.class);
                intent.putExtra(PARAM_MESSENGER, messenger);
                intent.putExtra(PARAM_AUTOFINISH_TRANSACTIONS, IapGooglePlay.this.autoFinishTransactions);
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
                IapGooglePlay.this.purchaseListener = listener;
                Intent intent = new Intent(activity, IapGooglePlayActivity.class);
                intent.putExtra(PARAM_MESSENGER, messenger);
                intent.putExtra(PARAM_AUTOFINISH_TRANSACTIONS, IapGooglePlay.this.autoFinishTransactions);
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
            p.put("state", IapJNI.TRANS_STATE_PURCHASED);

            // We check if orderId is actually set here, otherwise we return a blank string.
            // This is what Google used to do, but after some updates around June/May 2016
            // they stopped to include the orderId key at all for test purchases. See: DEF-1940
            if (p.has("orderId")) {
                p.put("trans_ident", p.get("orderId"));
            } else {
                p.put("trans_ident", "");
            }

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

            if (!items.containsKey(RESPONSE_INAPP_ITEM_LIST)) {
                return false;
            }

            ArrayList<String> ownedSkus = items.getStringArrayList(RESPONSE_INAPP_ITEM_LIST);
            ArrayList<String> purchaseDataList = items.getStringArrayList(RESPONSE_INAPP_PURCHASE_DATA_LIST);
            ArrayList<String> signatureList = items.getStringArrayList(RESPONSE_INAPP_SIGNATURE_LIST);
            for (int i = 0; i < ownedSkus.size(); ++i) {
                int c = IapJNI.BILLING_RESPONSE_RESULT_OK;
                String pd = convertPurchase(purchaseDataList.get(i), signatureList.get(i));
                if (pd == null) {
                    pd = "";
                    c = IapJNI.BILLING_RESPONSE_RESULT_ERROR;
                }
                purchaseListener.onPurchaseResult(c, pd);
            }
        }
        return true;
    }
}
