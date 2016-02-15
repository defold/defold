package com.defold.iap;

import java.text.SimpleDateFormat;
import java.util.Map;
import java.util.Set;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Date;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;

import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

import com.amazon.device.iap.PurchasingService;
import com.amazon.device.iap.PurchasingListener;
import com.amazon.device.iap.model.ProductDataResponse;
import com.amazon.device.iap.model.PurchaseUpdatesResponse;
import com.amazon.device.iap.model.PurchaseResponse;
import com.amazon.device.iap.model.UserDataResponse;
import com.amazon.device.iap.model.RequestId;
import com.amazon.device.iap.model.Product;
import com.amazon.device.iap.model.Receipt;
import com.amazon.device.iap.model.UserData;
import com.amazon.device.iap.model.FulfillmentResult;

public class IapAmazon implements PurchasingListener {

    public static final String TAG = "iap";

    private HashMap<RequestId, IListProductsListener> listProductsListeners;
    private HashMap<RequestId, IPurchaseListener> purchaseListeners;

    private Activity activity;
    private boolean autoFinishTransactions;

    public IapAmazon(Activity activity, boolean autoFinishTransactions) {
        this.activity = activity;
        this.autoFinishTransactions = autoFinishTransactions;
        this.listProductsListeners = new HashMap<RequestId, IListProductsListener>();
        this.purchaseListeners = new HashMap<RequestId, IPurchaseListener>();
        PurchasingService.registerListener(activity, this);
    }

    private void init() {
    }

    public void stop() {
    }

    public void listItems(final String skus, final IListProductsListener listener) {
        final Set<String> skuSet = new HashSet<String>();
        for (String x : skus.split(",")) {
            if (x.trim().length() > 0) {
                if (!skuSet.contains(x)) {
                    skuSet.add(x);
                }
            }
        }

        // It might seem unconventional to hold the lock while doing the function call,
        // but it prevents a race condition, as the API does not allow supplying own
        // requestId which could be generated ahead of time.
        synchronized (listProductsListeners) {
            RequestId req = PurchasingService.getProductData(skuSet);
            if (req != null) {
                listProductsListeners.put(req, listener);
            } else {
                Log.e(TAG, "Did not expect a null requestId");
            }
        }
    }

    public void buy(final String product, final IPurchaseListener listener) {
        synchronized (purchaseListeners) {
            RequestId req = PurchasingService.purchase(product);
            if (req != null) {
                purchaseListeners.put(req, listener);
            } else {
                Log.e(TAG, "Did not expect a null requestId");
            }
        }
    }

    public void finishTransaction(final String receipt, final IPurchaseListener listener) {
        if(this.autoFinishTransactions) {
            return;
        }
        PurchasingService.notifyFulfillment(receipt, FulfillmentResult.FULFILLED);
    }

    private void doGetPurchaseUpdates(final IPurchaseListener listener, final boolean reset) {
        synchronized (purchaseListeners) {
            RequestId req = PurchasingService.getPurchaseUpdates(reset);
            if (req != null) {
                purchaseListeners.put(req, listener);
            } else {
                Log.e(TAG, "Did not expect a null requestId");
            }
        }
    }

    public void processPendingConsumables(final IPurchaseListener listener) {
        // reset = false means getting any new receipts since the last call.
        doGetPurchaseUpdates(listener, false);
    }

    public void restore(final IPurchaseListener listener) {
        // reset = true means getting all transaction history, although consumables
        // are not included, only entitlements, after testing.
        doGetPurchaseUpdates(listener, true);
    }

    public static String toISO8601(final Date date) {
        String formatted = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ssZ").format(date);
        return formatted.substring(0, 22) + ":" + formatted.substring(22);
    }

    private JSONObject makeTransactionObject(final UserData user, final Receipt receipt, int state) throws JSONException {
        JSONObject transaction = new JSONObject();
        transaction.put("ident", receipt.getSku());
        transaction.put("state", state);
        transaction.put("date", toISO8601(receipt.getPurchaseDate()));
        transaction.put("trans_ident", receipt.getReceiptId());
        transaction.put("receipt", receipt.getReceiptId());

        // Only for amazon (this far), but required for using their server side receipt validation.
        transaction.put("is_sandbox_mode", PurchasingService.IS_SANDBOX_MODE);
        transaction.put("user_id", user.getUserId());

        // According to documentation, cancellation support has to be enabled per item, and this is
        // not officially supported by any other IAP provider, and it is not expected to be used here either.
        //
        // But enforcing the use of only non-cancelable items is not possible either; so include these flags
        // for completeness.
        if (receipt.getCancelDate() != null)
            transaction.put("cancel_date", toISO8601(receipt.getCancelDate()));
        transaction.put("canceled", receipt.isCanceled());
        return transaction;
    }

    // This callback method is invoked when an ProductDataResponse is available for a request initiated by PurchasingService.getProductData(java.util.Set).
    @Override
    public void onProductDataResponse(ProductDataResponse productDataResponse) {
        RequestId reqId = productDataResponse.getRequestId();
        IListProductsListener listener;
        synchronized (this.listProductsListeners) {
            listener = this.listProductsListeners.get(reqId);
            if (listener == null) {
                Log.e(TAG, "No listener found for request " + reqId.toString());
                return;
            }
            this.listProductsListeners.remove(reqId);
        }

        if (productDataResponse.getRequestStatus() != ProductDataResponse.RequestStatus.SUCCESSFUL) {
            listener.onProductsResult(IapJNI.BILLING_RESPONSE_RESULT_ERROR, null);
        } else {
            Map<String, Product> products = productDataResponse.getProductData();
            try {
                JSONObject data = new JSONObject();
                for (Map.Entry<String, Product> entry : products.entrySet()) {
                    String key = entry.getKey();
                    Product product = entry.getValue();
                    JSONObject item = new JSONObject();
                    item.put("ident", product.getSku());
                    item.put("title", product.getTitle());
                    item.put("description", product.getDescription());
                    if (product.getPrice() != null) {
                        String priceString = product.getPrice();
                        item.put("price_string", priceString);
                        // Based on return values from getPrice: https://developer.amazon.com/public/binaries/content/assets/javadoc/in-app-purchasing-api/com/amazon/inapp/purchasing/item.html
                        item.put("price", priceString.replaceAll("[^0-9.,]", ""));
                    }
                    data.put(key, item);
                }
                listener.onProductsResult(IapJNI.BILLING_RESPONSE_RESULT_OK, data.toString());
            } catch (JSONException e) {
                listener.onProductsResult(IapJNI.BILLING_RESPONSE_RESULT_ERROR, null);
            }
        }
    }

    // Convenience function for getting and removing a purchaseListener (used for more than one operation).
    private IPurchaseListener pickPurchaseListener(RequestId requestId) {
        synchronized (this.purchaseListeners) {
            IPurchaseListener listener = this.purchaseListeners.get(requestId);
            if (listener != null) {
                this.purchaseListeners.remove(requestId);
                return listener;
            }
        }
        return null;
    }

    // This callback method is invoked when a PurchaseResponse is available for a purchase initiated by PurchasingService.purchase(String).
    @Override
    public void onPurchaseResponse(PurchaseResponse purchaseResponse) {

        IPurchaseListener listener = pickPurchaseListener(purchaseResponse.getRequestId());
        if (listener == null) {
            Log.e(TAG, "No listener found for request: " + purchaseResponse.getRequestId().toString());
            return;
        }

        int code;
        String data = null;
        String fulfilReceiptId = null;

        switch (purchaseResponse.getRequestStatus()) {
            case SUCCESSFUL:
                {
                    try {
                        code = IapJNI.BILLING_RESPONSE_RESULT_OK;
                        data = makeTransactionObject(purchaseResponse.getUserData(), purchaseResponse.getReceipt(), IapJNI.TRANS_STATE_PURCHASED).toString();
                        fulfilReceiptId = purchaseResponse.getReceipt().getReceiptId();
                    } catch (JSONException e) {
                        Log.e(TAG, "JSON Exception occured: " + e.toString());
                        code = IapJNI.BILLING_RESPONSE_RESULT_DEVELOPER_ERROR;
                    }
                }
                break;
            case ALREADY_PURCHASED:
                code = IapJNI.BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED;
                break;
            case INVALID_SKU:
                code = IapJNI.BILLING_RESPONSE_RESULT_ITEM_UNAVAILABLE;
                break;
            case FAILED:
            case NOT_SUPPORTED:
            default:
                code = IapJNI.BILLING_RESPONSE_RESULT_ERROR;
                break;
        }

        listener.onPurchaseResult(code, data);

        if (fulfilReceiptId != null && autoFinishTransactions) {
            PurchasingService.notifyFulfillment(fulfilReceiptId, FulfillmentResult.FULFILLED);
        }
    }

    // This callback method is invoked when a PurchaseUpdatesResponse is available for a request initiated by PurchasingService.getPurchaseUpdates(boolean).
    @Override
    public void onPurchaseUpdatesResponse(PurchaseUpdatesResponse purchaseUpdatesResponse) {

        // The documentation seems to be a little misguiding regarding how to handle this.
        // This call is in response to getPurchaseUpdates() which can be called in two modes
        //
        //    1) Get all receipts since last call (reset = true)
        //    2) Get the whole transaction history.
        //
        // The result can carry the flag hasMore() where it is required to call getPurchaseUpdates again. See docs:
        // https://developer.amazon.com/public/apis/earn/in-app-purchasing/docs-v2/implementing-iap-2.0
        //
        // Examples indicate it should be called with the same value for 'reset' the secon time around
        // but actual testing ends up in an infinite loop where the same results are returned over and over.
        //
        // So here getPurchaseUpdates is called with result=false to fetch the next round of receipts.

        RequestId reqId = purchaseUpdatesResponse.getRequestId();
        IPurchaseListener listener = pickPurchaseListener(reqId);
        if (listener == null) {
            Log.e(TAG, "No listener found for request " + reqId.toString());
            return;
        }

        switch (purchaseUpdatesResponse.getRequestStatus()) {
            case SUCCESSFUL:
                {
                    try {
                        for (Receipt receipt : purchaseUpdatesResponse.getReceipts()) {
                            JSONObject trans = makeTransactionObject(purchaseUpdatesResponse.getUserData(), receipt, IapJNI.TRANS_STATE_PURCHASED);
                            listener.onPurchaseResult(IapJNI.BILLING_RESPONSE_RESULT_OK, trans.toString());
                            if(autoFinishTransactions) {
                                PurchasingService.notifyFulfillment(receipt.getReceiptId(), FulfillmentResult.FULFILLED);
                            }
                        }
                        if (purchaseUpdatesResponse.hasMore()) {
                            doGetPurchaseUpdates(listener, false);
                        }
                    } catch (JSONException e) {
                        Log.e(TAG, "JSON Exception occured: " + e.toString());
                        listener.onPurchaseResult(IapJNI.BILLING_RESPONSE_RESULT_DEVELOPER_ERROR, null);
                    }
                }
                break;
            case FAILED:
            case NOT_SUPPORTED:
            default:
                listener.onPurchaseResult(IapJNI.BILLING_RESPONSE_RESULT_ERROR, null);
                break;
        }
    }

    // This callback method is invoked when a UserDataResponse is available for a request initiated by PurchasingService.getUserData().
    @Override
    public void onUserDataResponse(UserDataResponse userDataResponse) {
        // Intentionally left un-implemented; not used.
    }
}
