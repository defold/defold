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
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

public class SSDP implements ISSDP {

    public interface Logger {
        void log(String msg);
    }

    private static final String SSDP_MCAST_ADDR_IP = "239.255.255.250";
    private static final int SSDP_MCAST_PORT = 1900;
    private static final int SSDP_MCAST_TTL = 4;
    private static final int SSDP_MAX_WAIT_TIME = 2;

    private Logger logger;
    private InetAddress SSDP_MCAST_ADDR;
    private List<NetworkInterface> interfaces;
    private MulticastSocket mcastSocket;
    private List<DatagramSocket> sockets;
    private byte[] buffer;
    private Map<String, DeviceInfo> discoveredDevices = new HashMap<String, DeviceInfo>();
    private int changeCount = 0;

    private static final String M_SEARCH_PAYLOAD =
            String.format("M-SEARCH * HTTP/1.1\r\n"
                    + "Host: %s:%d\r\n"
                    + "MAN: \"ssdp:discover\"\r\n"
                    + "MX: %d\r\n"
                    + "ST: upnp:rootdevice\r\n\r\n", SSDP_MCAST_ADDR_IP, SSDP_MCAST_PORT, SSDP_MAX_WAIT_TIME);

    static List<NetworkInterface> getMCastInterfaces() throws SocketException {
        List<NetworkInterface> interfaces = Collections.list(NetworkInterface.getNetworkInterfaces());
        List<NetworkInterface> r = new ArrayList<NetworkInterface>();

        for (NetworkInterface i : interfaces) {
            if (i.isUp() && !i.isLoopback() && !i.isPointToPoint() && !i.isVirtual() && i.supportsMulticast()) {
                r.add(i);
            }
        }

        return r;
    }

    static List<InetAddress> getIPv4Addresses(NetworkInterface i) throws SocketException {
        List<InetAddress> r = new ArrayList<InetAddress>();
        for (InetAddress a : Collections.list(i.getInetAddresses())) {
            if (a instanceof Inet4Address) {
                r.add(a);
            }
        }

        return r;
    }

    public SSDP(Logger logger) {
        this.logger = logger;
        buffer = new byte[1500];
    }

    private void log(String msg) {
        logger.log("SSDP: " + msg);
    }

    public boolean setup() throws IOException {
        try {
            SSDP_MCAST_ADDR = InetAddress.getByName(SSDP_MCAST_ADDR_IP);
            log("Started successfully");
        } catch (UnknownHostException e) {
            log("Fatal error: " + SSDP_MCAST_ADDR_IP);
            return false;
        }
        return true;
    }

    private void closeSockets() {
        if (sockets != null) {
            for (DatagramSocket socket : sockets) {
                socket.close();
            }
            sockets.clear();
        }
        if (mcastSocket != null) {
            mcastSocket.close();
            mcastSocket = null;
        }
    }

    private void registerDevice(String usn, DeviceInfo device) {
        DeviceInfo discDevice = discoveredDevices.get(usn);
        if (!device.equals(discDevice)) {
            ++changeCount;
        }
        discoveredDevices.put(usn, device);
    }

    private void refreshNetworks() throws IOException {
        List<NetworkInterface> newInterfaces = getMCastInterfaces();
        if (!newInterfaces.equals(this.interfaces)) {
            clearDiscovered();
            SSDP_MCAST_ADDR = InetAddress.getByName(SSDP_MCAST_ADDR_IP);
            for (NetworkInterface i : newInterfaces) {
                closeSockets();
                try {
                    mcastSocket = new MulticastSocket(SSDP_MCAST_PORT);
                    mcastSocket.setNetworkInterface(i);
                    mcastSocket.joinGroup(SSDP_MCAST_ADDR);
                    mcastSocket.setSoTimeout(1);
                    mcastSocket.setTimeToLive(SSDP_MCAST_TTL);
                    sockets = new ArrayList<DatagramSocket>();
                    List<InetAddress> addresses = getIPv4Addresses(i);
                    for (InetAddress a : addresses) {
                        DatagramSocket socket = new DatagramSocket(0, a);
                        socket.setSoTimeout(1);
                        sockets.add(socket);
                    }
                    log(String.format("Connected to multicast network %s: %s", i.getDisplayName(), addresses.toString()));
                    this.interfaces = newInterfaces;
                    break;
                } catch (IOException e) {
                    log("Could not connect: " + e.getMessage());
                    closeSockets();
                }
            }
        }
    }

    private void sendSearch() throws IOException {
        if (mcastSocket != null) {
            byte[] buf = M_SEARCH_PAYLOAD.getBytes();
            DatagramPacket p = new DatagramPacket(buf, buf.length, SSDP_MCAST_ADDR,
                    SSDP_MCAST_PORT);
            log("Searching for devices");
            for (DatagramSocket s : sockets) {
                try {
                    s.send(p);
                } catch (IOException e) {
                    log(String.format("Searching failed on %s: %s", s.getLocalAddress(), e.getMessage()));
                    // Might get no route to host etc on an interface, but
                    // that is no good reason to stop searching there, so just
                    // ignore and continue with next interface.
                }
            }
        } else {
            log("Could not find a multicast network");
        }
    }

    private void expireDiscovered() {
        long now = System.currentTimeMillis();
        Set<String> toExpire = new HashSet<String>();
        for (Entry<String, DeviceInfo> e : discoveredDevices.entrySet()) {
            DeviceInfo dev = e.getValue();
            if (now >= dev.expires) {
                log(String.format("[%s] has expired and was removed", dev.address));
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
            refreshNetworks();
            sendSearch();
        }

        expireDiscovered();

        if (mcastSocket != null) {
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
        }
        return changeCount != oldChangeCount;
    }

    private void handleRequest(String data, String address) {
        Request request = Request.parse(data);
        if (request == null) {
            log(String.format("[%s] Invalid request: %s", address, data));
            return;
        }

        if (request.method.equals("NOTIFY")) {
            String usn = request.headers.get("USN");
            if (usn != null || !request.headers.containsKey("NTS")) {
                DeviceInfo device = DeviceInfo.create(request.headers, address);
                // alive vs byebye in NTS field?
                String nts = request.headers.get("NTS");
                if (nts.equals("ssdp:alive")) {
                    registerDevice(usn, device);
                } else if (nts.equals("ssdp:byebye")) {
                    ++changeCount;
                    discoveredDevices.remove(usn);
                } else {
                    log(String.format("[%s] Unsupported NOTIFY response: %s", address, nts));
                }
            } else {
                log(String.format("[%s] Malformed NOTIFY response: %s", address, data));
            }
        }
        // We ignore M-SEARCH requests
    }

    private void handleResponse(String data, String address) {
        Response response = Response.parse(data);
        if (response == null) {
            log(String.format("[%s] Invalid response: %s", address, data));
            return;
        }

        if (response.statusCode == 200) {
            String usn = response.headers.get("USN");
            if (usn != null) {
                DeviceInfo device = DeviceInfo.create(response.headers, address);
                registerDevice(usn, device);
            } else {
                log(String.format("[%s] Malformed response: %s", address, data));
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
        closeSockets();
        log("Stopped successfully");
    }

    @Override
    public void clearDiscovered() {
        discoveredDevices.clear();
    }

    @Override
    public boolean isConnected() {
        return mcastSocket != null && !mcastSocket.isClosed();
    }

    @Override
    public List<String> getIPs() {
        if (sockets != null) {
            List<String> ips = new ArrayList<String>(sockets.size());
            for (DatagramSocket s : sockets) {
                ips.add(s.getLocalAddress().getHostAddress());
            }
            return ips;
        } else {
            return null;
        }
    }
}
