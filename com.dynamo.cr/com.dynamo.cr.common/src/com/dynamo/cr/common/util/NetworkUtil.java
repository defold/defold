package com.dynamo.cr.common.util;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.util.Collection;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.Set;
import java.util.Vector;

public class NetworkUtil {

    /**
     * Returns a list of the local inet addresses which are IPv4.
     * 
     * @return list of local inet addresses
     * @throws SocketException
     * @throws UnknownHostException
     */
    public static Collection<InetAddress> getValidHostAddresses() throws SocketException, UnknownHostException {
        Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces();
        Vector<InetAddress> result = new Vector<InetAddress>();
        while (interfaces.hasMoreElements()) {
            NetworkInterface netInterface = interfaces.nextElement();
            if (netInterface.isUp()) {
                Enumeration<InetAddress> addresses = netInterface.getInetAddresses();
                while (addresses.hasMoreElements()) {
                    InetAddress address = addresses.nextElement();
                    if (address instanceof Inet4Address) {
                        result.add(address);
                    }
                }
            }
        }
        if (result.isEmpty()) {
            return Collections.singleton(InetAddress.getByName("127.0.0.1"));
        }
        return result;
    }

    /**
     * Returns the address which most closely matches the target address.
     * 
     * @return Localhost IP
     * @throws SocketException
     * @throws UnknownHostException
     */
    public static InetAddress getClosestAddress(Collection<InetAddress> addresses, InetAddress targetAddress)
                                                                                                             throws SocketException,
                                                                                                             UnknownHostException {
        byte[] rawTarget = targetAddress.getAddress();
        Set<InetAddress> remaining = new HashSet<InetAddress>(addresses);
        if (remaining.isEmpty()) {
            return InetAddress.getByName("127.0.0.1");
        }
        for (int i = 0; i < rawTarget.length && remaining.size() > 1; ++i) {
            for (InetAddress address : remaining) {
                if (address.getAddress()[i] != rawTarget[i]) {
                    remaining.remove(address);
                    if (remaining.size() == 1) {
                        break;
                    }
                }
            }
        }
        return remaining.iterator().next();
    }
}
