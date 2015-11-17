package com.defold.iac;

public class IACJNI implements IIACListener {

    public IACJNI() {
    }

    @Override
    public native void onInvocation(String payload, String origin);
}
