package com.defold.push;

public class PushJNI implements IPushListener {

    public PushJNI() {
    }

    @Override
    public native void onMessage(String json);

    @Override
    public native void onRegistration(String regid, String errorMessage);

}
