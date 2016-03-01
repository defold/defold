package com.dynamo.upnp;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.MulticastSocket;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;
import java.util.logging.Logger;


public class SSDP implements ISSDP {

    private Logger logger = Logger.getLogger(SSDP.class.getCanonicalName());

    private static final String SSDP_MCAST_ADDR_IP = "239.255.255.250";
    private static final int SSDP_MCAST_PORT = 1900;
    private static final int SSDP_MCAST_TTL = 4;

    private final InetAddress SSDP_MCAST_ADDR;
    private MulticastSocket mcastSocket;
    private DatagramSocket[] sockets;
    private byte[] buffer;
    private Map<String, DeviceInfo> discoveredDevices = new HashMap<String, DeviceInfo>();
    private int changeCount = 0;
    private long timeOutThreshold = 0;

    private static final String M_SEARCH_PAYLOAD =
              "M-SEARCH * HTTP/1.1\r\n"
            + "Host: 239.255.255.250:1900\r\n"
            + "MAN: \"ssdp:discover\"\r\n"
            + "MX: 3\r\n"
            + "ST: upnp:rootdevice\r\n\r\n";

    static List<InetAddress> getLocalAddresses() throws SocketException {
        List<NetworkInterface> interfaces = Collections.list(NetworkInterface.getNetworkInterfaces());
        List<InetAddress> r = new ArrayList<InetAddress>();

        for (NetworkInterface i : interfaces) {
            if (i.isUp() && !i.isLoopback() && !i.isPointToPoint() && !i.isVirtual()) {
                for (InetAddress a : Collections.list(i.getInetAddresses())) {
                    if (a instanceof Inet4Address) {
                        r.add(a);
                    }
                }
            }
        }

        return r;
    }

    public SSDP() throws IOException {
        buffer = new byte[1500];
        SSDP_MCAST_ADDR = InetAddress.getByName(SSDP_MCAST_ADDR_IP);

        List<InetAddress> localAddresses = getLocalAddresses();
        sockets = new DatagramSocket[localAddresses.size()];
        int i = 0;
        for (InetAddress inetAddress : localAddresses) {
            DatagramSocket socket = new DatagramSocket();
            socket = new DatagramSocket(0, inetAddress);
            socket.setSoTimeout(1);
            sockets[i++] = socket;
        }

        mcastSocket = new MulticastSocket(SSDP_MCAST_PORT);
        mcastSocket.joinGroup(SSDP_MCAST_ADDR);
        mcastSocket.setSoTimeout(1);
        mcastSocket.setTimeToLive(SSDP_MCAST_TTL);
    }

    private void sendSearch() {
        byte[] buf = M_SEARCH_PAYLOAD.getBytes();
        DatagramPacket p = new DatagramPacket(buf, buf.length, SSDP_MCAST_ADDR,
                SSDP_MCAST_PORT);
        for (DatagramSocket s : sockets) {
            try {
                s.send(p);
            } catch (IOException e) {
                // Might get no route to host etc on an interface, but
                // that is no good reason to stop searching there, so just
                // ignore and continue with next interface.
            }
        }
    }

    private void expireDiscovered() {
        long now = System.currentTimeMillis();
        Set<String> toExpire = new HashSet<String>();
        for (Entry<String, DeviceInfo> e : discoveredDevices.entrySet()) {
            DeviceInfo dev = e.getValue();
            if (now >= dev.expires) {
                toExpire.add(e.getKey());
            }
        }
        for (String id : toExpire) {
            ++changeCount;
            discoveredDevices.remove(id);
        }
    }

    @Override
    public boolean update(boolean search) throws IOException {
        int oldChangeCount = changeCount;
        if (search) {
            sendSearch();
        }

        expireDiscovered();

        boolean cont = true;
        do {
            try {
                DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                mcastSocket.receive(packet);
                String data = new String(buffer, 0, packet.getLength());
                handleRequest(data, packet.getAddress().getHostAddress());
            } catch (SocketTimeoutException e) {
                // Ignore
                cont = false;
            }
        } while (cont);

        cont = true;
        do {
            for (DatagramSocket socket : sockets) {
                try {
                    DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                    socket.receive(packet);
                    String data = new String(buffer, 0, packet.getLength());
                    handleResponse(data, packet.getAddress().getHostAddress());

                } catch (SocketTimeoutException e) {
                    // Ignore
                    cont = false;
                }
            }
        } while (cont);

        return changeCount != oldChangeCount;
    }

    private void handleRequest(String data, String address) {
        Request request = Request.parse(data);
        if (request == null) {
            logger.warning("Invalid request: " + data);
            return;
        }

        if (request.method.equals("NOTIFY")) {
            String usn = request.headers.get("USN");
            if (usn != null) {
                DeviceInfo device = DeviceInfo.create(request.headers, address);
                if (device != null) {
                    // alive vs byebye in NTS field?
                    String nts = request.headers.get("NTS");
                    if (nts != null && nts.equals("ssdp:alive")) {
                        DeviceInfo discDevice = discoveredDevices.get(usn);
                        if (discDevice == null) {
                            discoveredDevices.put(usn, device);
                            ++changeCount;
                        } else {
                            // The address might have changed (for identical usn) so we check
                            // for equality here (equals in DeviceInfo compares headers)
                            if (!discDevice.equals(device)) {
                                discoveredDevices.put(usn, device);
                                ++changeCount;
                            }
                        }
                    } else {
                        ++changeCount;
                        discoveredDevices.remove(usn);
                    }
                } else {
                    logger.warning("Malformed NOTIFY response " + data);
                }
            }
        }
        // We ignore M-SEARCH requests
    }

    private void handleResponse(String data, String address) {
        Response response = Response.parse(data);
        if (response == null) {
            logger.warning("Invalid response: " + data);
            return;
        }

        if (response.statusCode == 200) {
            String usn = response.headers.get("USN");
            if (usn != null) {
                DeviceInfo device = DeviceInfo.create(response.headers, address);
                if (device != null) {
                    DeviceInfo discDevice = discoveredDevices.get(usn);
                    if (discDevice == null) {
                        discoveredDevices.put(usn, device);
                        ++changeCount;
                    } else {
                        // The port might have changed (for identical usn) so we check
                        // for equality here (equals in DeviceInfo compares headers)
                        if (!discDevice.equals(device)) {
                            discoveredDevices.put(usn, device);
                            ++changeCount;
                        }
                    }
                } else {
                    logger.warning("Malformed response " + data);
                }
            }
        }
    }

    @Override
    public DeviceInfo getDeviceInfo(String usn) {
        return discoveredDevices.get(usn);
    }

    @Override
    public DeviceInfo[] getDevices() {
        return discoveredDevices.values().toArray(new DeviceInfo[discoveredDevices.size()]);
    }

    @Override
    public void dispose() {
        for (DatagramSocket socket : sockets) {
            socket.close();
        }
        this.mcastSocket.close();
    }

    @Override
    public void clearDiscovered() {
        discoveredDevices.clear();
    }
}
