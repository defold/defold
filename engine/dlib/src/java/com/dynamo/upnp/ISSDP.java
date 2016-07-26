package com.dynamo.upnp;


import java.io.IOException;

public interface ISSDP {

    public boolean setup() throws IOException;

    public boolean update(boolean search) throws IOException;

    public DeviceInfo getDeviceInfo(String usn);

    public void dispose();

    public void clearDiscovered();

    public DeviceInfo[] getDevices();

    public boolean isConnected();
}
