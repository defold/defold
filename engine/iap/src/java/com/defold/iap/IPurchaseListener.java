package com.defold.iap;

public interface IPurchaseListener {
	public void onResult(int responseCode, String purchaseData, String dataSignature);
}
