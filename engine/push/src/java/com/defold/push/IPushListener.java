package com.defold.push;

public interface IPushListener {
    public void onRegistration(String regid, String errorMessage);

    public void onMessage(String json, boolean wasActivated);
    public void onLocalMessage(String json, int id, boolean wasActivated);
}
