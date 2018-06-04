package com.defold.push;

public interface IPushListener {
    public void onRegistration(String regid, String errorMessage);
    public void onMessage(String json, boolean wasActivated);
    public void onLocalMessage(String json, int id, boolean wasActivated);
    public void addPendingNotifications(int uid, String title, String message, String payload, long timestamp, int priority);
}
