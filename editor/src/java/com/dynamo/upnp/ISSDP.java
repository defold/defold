package com.dynamo.upnp;

import java.io.IOException;
import java.util.List;

public interface ISSDP {

    public boolean update(boolean search) throws IOException;

    public DeviceInfo getDeviceInfo(String usn);

    public void dispose();

    public void clearDiscovered();

    public DeviceInfo[] getDevices();

    public boolean isConnected();

    public List<String> getIPs();
}