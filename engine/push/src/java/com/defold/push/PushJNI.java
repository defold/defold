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

    @Override
    public native void addPendingNotifications(int uid, String title, String message, String payload, long timestamp, int priority);
}
