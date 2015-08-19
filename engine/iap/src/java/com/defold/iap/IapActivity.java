package com.defold.iap;

import java.util.ArrayList;

import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender.SendIntentException;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup.LayoutParams;

import com.android.vending.billing.IInAppBillingService;
import com.defold.iap.Iap.Action;

public class IapActivity extends Activity {

    private Messenger messenger;
    ServiceConnection serviceConn;
    IInAppBillingService service;

    // NOTE: Code from "trivialdrivesample"
    int getResponseCodeFromBundle(Bundle b) {
        Object o = b.get(Iap.RESPONSE_CODE);
        if (o == null) {
            Log.d(Iap.TAG, "Bundle with null response code, assuming OK (known issue)");
            return Iap.BILLING_RESPONSE_RESULT_OK;
        } else if (o instanceof Integer)
            return ((Integer) o).intValue();
        else if (o instanceof Long)
            return (int) ((Long) o).longValue();
        else {
            Log.e(Iap.TAG, "Unexpected type for bundle response code.");
            Log.e(Iap.TAG, o.getClass().getName());
            throw new RuntimeException("Unexpected type for bundle response code: " + o.getClass().getName());
        }
    }

    private void sendBuyError(int error) {
        Bundle bundle = new Bundle();
        bundle.putString("action", Action.BUY.toString());

        bundle.putInt(Iap.RESPONSE_CODE, error);
        Message msg = new Message();
        msg.setData(bundle);

        try {
            messenger.send(msg);
        } catch (RemoteException e) {
            Log.wtf(Iap.TAG, "Unable to send message", e);
        }
        this.finish();
    }

    private void buy(String product) {
        try {
            Bundle buyIntentBundle = service.getBuyIntent(3, getPackageName(), product, "inapp", "");
            int response = getResponseCodeFromBundle(buyIntentBundle);

            if (response == Iap.BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED) {
                // NOTE: This should never happen. All products should already have been consumed..
                Log.e(Iap.TAG, String.format("Unexpected product %s in inventory. Consuming before purchase", product));
                consumeProduct(product);
                try {
                    Thread.sleep(2000);

                    buyIntentBundle = service.getBuyIntent(3, getPackageName(), product, "inapp", "");
                    response = getResponseCodeFromBundle(buyIntentBundle);
                } catch (InterruptedException e) {
                    Log.wtf(Iap.TAG, "Failed to sleep", e);
                }
            }

            if (response == Iap.BILLING_RESPONSE_RESULT_OK) {
                PendingIntent pendingIntent = buyIntentBundle.getParcelable("BUY_INTENT");
                startIntentSenderForResult(pendingIntent.getIntentSender(), 1001, new Intent(), Integer.valueOf(0), Integer.valueOf(0), Integer.valueOf(0));
            }  else {
                sendBuyError(response);
            }

        } catch (RemoteException e) {
            Log.e(Iap.TAG, String.format("Failed to buy", e));
            sendBuyError(Iap.BILLING_RESPONSE_RESULT_ERROR);
        } catch (SendIntentException e) {
            Log.e(Iap.TAG, String.format("Failed to buy", e));
            sendBuyError(Iap.BILLING_RESPONSE_RESULT_ERROR);
        }
    }

    private void consume(String purchaseData) {
        try {
            JSONObject pd = new JSONObject(purchaseData);
            String token = pd.getString("purchaseToken");

            int consumeResponse = service.consumePurchase(3, getPackageName(), token);
            if (consumeResponse != Iap.BILLING_RESPONSE_RESULT_OK) {
                Log.e(Iap.TAG, String.format("Failed to consume purchase (%d)", consumeResponse));
            }
        } catch (RemoteException e) {
            Log.e(Iap.TAG, "Failed to consume purchase", e);
        } catch (JSONException e) {
            Log.e(Iap.TAG, "Failed to consume purchase", e);
        }
    }

    private void consumeProduct(String product) {
        int response = Iap.BILLING_RESPONSE_RESULT_ERROR;
        Bundle bundle = new Bundle();
        bundle.putString("action", Action.RESTORE.toString());

        try {
            Bundle items = service.getPurchases(3, getPackageName(), "inapp", null);
            response = getResponseCodeFromBundle(items);

            if (response == Iap.BILLING_RESPONSE_RESULT_OK) {
                bundle.putBundle("items", items);

                ArrayList<String> ownedSkus = items.getStringArrayList(Iap.RESPONSE_INAPP_PURCHASE_DATA_LIST);
                for (String s : ownedSkus) {
                    JSONObject pd = new JSONObject(s);
                    if (product.equals(pd.getString("productId"))) {
                        consume(s);
                    }
                }
            }
        } catch (RemoteException e) {
            Log.e(Iap.TAG, "Failed to consume purchase", e);
        } catch (JSONException e) {
            Log.e(Iap.TAG, "Failed to consume purchase", e);
        }
    }

    private void restore() {
        int response = Iap.BILLING_RESPONSE_RESULT_ERROR;
        Bundle bundle = new Bundle();
        bundle.putString("action", Action.RESTORE.toString());

        try {
            Bundle items = service.getPurchases(3, getPackageName(), "inapp", null);
            response = getResponseCodeFromBundle(items);

            if (response == Iap.BILLING_RESPONSE_RESULT_OK) {
                bundle.putBundle("items", items);
            }
        } catch (RemoteException e) {
            Log.e(Iap.TAG, "Failed to restore purchases", e);
        }

        bundle.putInt(Iap.RESPONSE_CODE, response);
        Message msg = new Message();
        msg.setData(bundle);

        try {
            messenger.send(msg);
        } catch (RemoteException e) {
            Log.wtf(Iap.TAG, "Unable to send message", e);
        }
        this.finish();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        View view = new View(this);
        view.setBackgroundColor(0x10ffffff);
        setContentView(view, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        Intent intent = getIntent();
        final Bundle extras = intent.getExtras();
        this.messenger = (Messenger) extras.getParcelable(Iap.PARAM_MESSENGER);
        final Action action = Action.valueOf(intent.getAction());

        serviceConn = new ServiceConnection() {
            @Override
            public void onServiceDisconnected(ComponentName name) {
                service = null;
            }

            @Override
            public void onServiceConnected(ComponentName name, IBinder serviceBinder) {
                service = IInAppBillingService.Stub.asInterface(serviceBinder);
                if (action == Action.BUY) {
                    buy(extras.getString(Iap.PARAM_PRODUCT));
                } else if (action == Action.RESTORE) {
                    restore();
                }
            }
        };

        bindService(new Intent("com.android.vending.billing.InAppBillingService.BIND"), serviceConn, Context.BIND_AUTO_CREATE);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (serviceConn != null) {
            unbindService(serviceConn);
        }
    }

    // NOTE: Code from "trivialdrivesample"
    int getResponseCodeFromIntent(Intent i) {
        Object o = i.getExtras().get(Iap.RESPONSE_CODE);
        if (o == null) {
            Log.e(Iap.TAG, "Intent with no response code, assuming OK (known issue)");
            return Iap.BILLING_RESPONSE_RESULT_OK;
        } else if (o instanceof Integer) {
            return ((Integer) o).intValue();
        } else if (o instanceof Long) {
            return (int) ((Long) o).longValue();
        } else {
            Log.e(Iap.TAG, "Unexpected type for intent response code.");
            Log.e(Iap.TAG, o.getClass().getName());
            throw new RuntimeException("Unexpected type for intent response code: " + o.getClass().getName());
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        Bundle bundle = new Bundle();
        bundle.putString("action", Action.BUY.toString());

        if (data == null) {
            bundle.putInt(Iap.RESPONSE_CODE, Iap.BILLING_RESPONSE_RESULT_ERROR);
            Log.e(Iap.TAG, "Null data in IAB activity result.");
        } else {
            int responseCode = getResponseCodeFromIntent(data);
            String purchaseData = data.getStringExtra(Iap.RESPONSE_INAPP_PURCHASE_DATA);
            String dataSignature = data.getStringExtra(Iap.RESPONSE_INAPP_SIGNATURE);
            bundle.putInt(Iap.RESPONSE_CODE, responseCode);
            bundle.putString(Iap.RESPONSE_INAPP_PURCHASE_DATA, purchaseData);
            bundle.putString(Iap.RESPONSE_INAPP_SIGNATURE, dataSignature);

            // We always consume products
            if (purchaseData != null) {
                // Can be null if the store isn't configured correctly
                consume(purchaseData);
            }
        }

        Message msg = new Message();
        msg.setData(bundle);
        try {
            messenger.send(msg);
        } catch (RemoteException e) {
            Log.wtf(Iap.TAG, "Unable to send message", e);
        }

        this.finish();
    }
}



