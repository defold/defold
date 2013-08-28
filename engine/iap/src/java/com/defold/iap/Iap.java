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
    public static final int BILLING_RESPONSE_RESULT_BILLING_UNAVAILABLE = 3;
    public static final int BILLING_RESPONSE_RESULT_ITEM_UNAVAILABLE = 4;
    public static final int BILLING_RESPONSE_RESULT_DEVELOPER_ERROR = 5;
    public static final int BILLING_RESPONSE_RESULT_ERROR = 6;
    public static final int BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED = 7;
    public static final int BILLING_RESPONSE_RESULT_ITEM_NOT_OWNED = 8;
    
    public static enum Action {
    	BUY,
    	RESTORE,
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
					String res = listItemsSync(sr.skuList);
					sr.listener.onResult(res);
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
				service = null;
			}

			@Override
			public void onServiceConnected(ComponentName name, IBinder binderService) {
				service = IInAppBillingService.Stub.asInterface(binderService);
			}
		};

		activity.bindService(new Intent(
				"com.android.vending.billing.InAppBillingService.BIND"),
				serviceConn, Context.BIND_AUTO_CREATE);
		
		skuDetailsThread = new SkuDetailsThread();
		skuDetailsThread.start();		
	}
	
	public void stop() {
		if (serviceConn != null) {
			activity.unbindService(serviceConn);
		}
		if (skuDetailsThread != null) {
			skuDetailsThread.interrupt();
			try {
				skuDetailsThread.join();
			} catch (InterruptedException e) {
				Log.wtf(TAG, "Failed to join thread", e);
			}			
		}
	}

	public void listItems(final String skus, final IListProductsListener listener) {
		this.activity.runOnUiThread(new Runnable() {

			@Override
			public void run() {
				init();
				
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
			}
		});	
	}
	
	private static String convertProduct(String purchase) {
		try {
			JSONObject p = new JSONObject(purchase);			
			p.put("price_string", p.get("price"));
			p.put("ident", p.get("productId"));

			p.remove("productId");
			p.remove("type");
			p.remove("price");
			p.remove("price_amount_micros");
			p.remove("price_currency_code");
			return p.toString();			
		} catch (JSONException e) {
			Log.wtf(TAG, "Failed to convert product json", e);
		}
		
		return null;
	}

	private String listItemsSync(ArrayList<String> skuList)  {
		Bundle querySkus = new Bundle();
		querySkus.putStringArrayList("ITEM_ID_LIST", skuList);
		StringBuilder sb = new StringBuilder();
		sb.append("[");
		
		try {
			Bundle skuDetails = service.getSkuDetails(3, activity.getPackageName(), "inapp", querySkus);
			int response = skuDetails.getInt("RESPONSE_CODE");

			if (response == 0) {
				ArrayList<String> responseList = skuDetails.getStringArrayList("DETAILS_LIST");
				
				for (String r : responseList) {
					String p = convertProduct(r);
					if (p != null) {
						sb.append(p);						
					}
				}
			}

		} catch (RemoteException e) {
			Log.e(TAG, "Failed to fetch product list", e);
		}
		sb.append("]");

		return sb.toString();
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
	
	private static String convertPurchase(String purchase) {
		try {
			JSONObject p = new JSONObject(purchase);			
			p.put("ident", p.get("productId"));
			p.put("state", TRANS_STATE_PURCHASED);
			p.put("trans_ident", p.get("purchaseToken"));
			p.put("date", toISO8601(new Date(p.getInt("purchaseTime"))));
			p.put("receipt", ""); // TODO: How?
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
			
			String pd = "";
			if (purchaseData != null) {
				pd = convertPurchase(purchaseData);
			} else {
				responseCode = BILLING_RESPONSE_RESULT_ERROR;				
			}
			
			purchaseListener.onResult(responseCode, pd, dataSignature);
			this.purchaseListener = null;
		} else if (action == Action.RESTORE) {
			Bundle items = bundle.getBundle("items");
			ArrayList<String> ownedSkus = items.getStringArrayList(RESPONSE_INAPP_ITEM_LIST);
			ArrayList<String> purchaseDataList = items.getStringArrayList(RESPONSE_INAPP_PURCHASE_DATA_LIST);
			ArrayList<String> signatureList = items.getStringArrayList(RESPONSE_INAPP_SIGNATURE_LIST);
			for (int i = 0; i < ownedSkus.size(); ++i) {
				int c = BILLING_RESPONSE_RESULT_OK;
				String pd = convertPurchase(purchaseDataList.get(i));
				if (pd == null) {
					pd = "";
					c = BILLING_RESPONSE_RESULT_ERROR;
				}
				purchaseListener.onResult(c, pd, signatureList.get(i));
			}
		}	
		return true;
	}
}
