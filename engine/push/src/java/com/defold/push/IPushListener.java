package com.defold.push;

public interface IPushListener {
    public void onRegistration(String regid, String errorMessage);

    public void onMessage(String json);
    public void onLocalMessage(String json, int id);
}
