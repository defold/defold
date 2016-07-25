package com.dynamo.cr.target.core;

import java.net.InetAddress;

public interface ITarget {

    public String getName();

    public String getId();

    public InetAddress getInetAddress();

    public String getUrl();

    public int getLogPort();

    public String getLocalAddress();
}
