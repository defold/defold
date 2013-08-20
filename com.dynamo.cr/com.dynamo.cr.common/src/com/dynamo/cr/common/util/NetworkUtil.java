package com.dynamo.cr.common.util;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.util.Enumeration;

public class NetworkUtil {

    private static String localHost = null;

    /**
     * Returns the IP of the host on the network.
     * 
     * Traverses the network interfaces and returns the first address which is IPv4 and not loop back. Once found, the
     * IP is statically cached for the remainder of the application life cycle. If no IP could be found, it fallbacks to
     * 127.0.0.1
     * 
     * @return Localhost IP
     * @throws SocketException
     */
    public static String getHostAddress() throws SocketException {
        if (localHost == null) {
            Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces();
            while (interfaces.hasMoreElements()) {
                NetworkInterface netInterface = interfaces.nextElement();
                Enumeration<InetAddress> addresses = netInterface.getInetAddresses();
                while (addresses.hasMoreElements()) {
                    InetAddress address = addresses.nextElement();
                    if (!address.isLoopbackAddress() && address instanceof Inet4Address) {
                        localHost = address.getHostAddress();
                        break;
                    }
                }
            }
        }
        if (localHost != null) {
            return localHost;
        }
        return "127.0.0.1";
    }
}
