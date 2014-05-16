package com.defold.iap;

public interface IPurchaseListener {
	public void onPurchaseResult(int responseCode, String purchaseData);
}
