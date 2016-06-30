package com.defold.push;

public interface IPushListener {
    public void onRegistration(String regid, String errorMessage);

    public void onMessage(String json, boolean state);
    public void onLocalMessage(String json, int id, boolean state);
}
