package com.defold.push;

public class PushJNI implements IPushListener {

    public PushJNI() {
    }

    @Override
    public native void onMessage(String json, boolean wasActivated);

    @Override
    public native void onLocalMessage(String json, int id, boolean wasActivated);

    @Override
    public native void onRegistration(String regid, String errorMessage);

}
