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
import java.util.logging.Level;
import java.util.logging.Logger;

import com.dynamo.upnp.Request;
import com.dynamo.upnp.Response;

public class SSDP implements ISSDP {

    private Logger logger = Logger.getLogger(SSDP.class.getCanonicalName());

    private class Connection {

        public boolean connect(NetworkInterface networkInterface) throws IOException {
            List<InetAddress> addresses = getIPv4Addresses(networkInterface);
            if (!addresses.isEmpty()) {
                mcastSocket = new MulticastSocket(SSDP_MCAST_PORT);
                mcastSocket.setNetworkInterface(networkInterface);
                mcastSocket.joinGroup(SSDP_MCAST_ADDR);
                mcastSocket.setSoTimeout(1);
                mcastSocket.setTimeToLive(SSDP_MCAST_TTL);
                sockets = new ArrayList<DatagramSocket>();
                for (InetAddress a : addresses) {
                    DatagramSocket socket = new DatagramSocket(0, a);
                    socket.setSoTimeout(1);
                    sockets.add(socket);
                }
                log(String.format("Connected to multicast network %s: %s", networkInterface.getDisplayName(), addresses.toString()));
                return true;
            } else {
                return false;
            }
        }

        public void disconnect() throws IOException {
            if (sockets != null) {
                for (DatagramSocket socket : sockets) {
                    if (socket != null) {
                        socket.close();
                    }
                }
                sockets = null;
            }
            if (mcastSocket != null) {
                mcastSocket.leaveGroup(SSDP_MCAST_ADDR);
                mcastSocket.close();
                mcastSocket = null;
            }
        }

        private MulticastSocket mcastSocket;
        private List<DatagramSocket> sockets;
    }

    private static final String SSDP_MCAST_ADDR_IP = "239.255.255.250";
    private static final int SSDP_MCAST_PORT = 1900;
    private static final int SSDP_MCAST_TTL = 4;
    private static final int SSDP_MAX_WAIT_TIME = 2;

    private InetAddress SSDP_MCAST_ADDR;
    private List<NetworkInterface> interfaces;
    private List<Connection> connections;
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

    public SSDP() {
        buffer = new byte[1500];
        this.connections = new ArrayList<Connection>();
        this.logger.setLevel(Level.INFO);
    }

    @Override
    public void enableLogTracing(boolean logTracing) {
        if (logTracing) {
            this.logger.setLevel(Level.FINE);
        } else {
            this.logger.setLevel(Level.INFO);
        }
    }

    private void log(String msg) {
        logger.info(msg);
    }

    private void trace(String msg) {
        logger.fine(msg);
    }

    @Override
    public boolean setup() throws IOException {
        try {
            SSDP_MCAST_ADDR = InetAddress.getByName(SSDP_MCAST_ADDR_IP);
            log("Started successfully");
            refreshNetworks();
        } catch (UnknownHostException e) {
            log("Fatal error: " + SSDP_MCAST_ADDR_IP);
            return false;
        }
        return true;
    }

    private void closeConnections() throws IOException {
        for (Connection c : this.connections) {
            c.disconnect();
        }
        this.connections.clear();
    }

    private void registerDevice(String usn, DeviceInfo device) {
        DeviceInfo discDevice = discoveredDevices.get(usn);
        if (!device.equals(discDevice)) {
            trace(String.format("[%s] was discovered", device.address));
            ++changeCount;
        }
        discoveredDevices.put(usn, device);
    }

    private void refreshNetworks() throws IOException {
        List<NetworkInterface> newInterfaces = getMCastInterfaces();
        if (!newInterfaces.equals(this.interfaces)) {
            this.interfaces = newInterfaces;
            clearDiscovered();
            SSDP_MCAST_ADDR = InetAddress.getByName(SSDP_MCAST_ADDR_IP);
            closeConnections();
            for (NetworkInterface i : this.interfaces) {
                Connection c = new Connection();
                try {
                    if (c.connect(i)) {
                        this.connections.add(c);
                    }
                } catch (IOException e) {
                    log("Could not connect: " + e.getMessage());
                    c.disconnect();
                }
            }
        }
    }

    private void sendSearch() throws IOException {
        for (Connection c : this.connections) {
            if (c.mcastSocket != null) {
                byte[] buf = M_SEARCH_PAYLOAD.getBytes();
                DatagramPacket p = new DatagramPacket(buf, buf.length, SSDP_MCAST_ADDR,
                        SSDP_MCAST_PORT);
                trace("Searching for devices");
                for (DatagramSocket s : c.sockets) {
                    try {
                        s.send(p);
                    } catch (IOException e) {
                        trace(String.format("Searching failed on %s: %s", s.getLocalAddress(), e.getMessage()));
                        // Might get no route to host etc on an interface, but
                        // that is no good reason to stop searching there, so just
                        // ignore and continue with next interface.
                    }
                }
            }
        }
        if (this.connections.isEmpty()) {
            log("Could not find a multicast network");
        }
    }

    private void expireDiscovered() {
        long now = System.currentTimeMillis();
        Set<String> toExpire = new HashSet<String>();
        for (Entry<String, DeviceInfo> e : discoveredDevices.entrySet()) {
            DeviceInfo dev = e.getValue();
            if (now >= dev.expires) {
                trace(String.format("[%s] has expired and was removed", dev.address));
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

        for (Connection c : this.connections) {
            if (c.mcastSocket != null) {
                boolean cont = true;
                do {
                    try {
                        DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                        c.mcastSocket.receive(packet);
                        String data = new String(buffer, 0, packet.getLength());
                        InetAddress localAddress = getIPv4Addresses(c.mcastSocket.getNetworkInterface()).get(0);
                        handleRequest(data, packet.getAddress().getHostAddress(), localAddress.getHostAddress());
                    } catch (SocketTimeoutException e) {
                        // Ignore
                        cont = false;
                    }
                } while (cont);
                cont = true;
                do {
                    for (DatagramSocket socket : c.sockets) {
                        try {
                            DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                            socket.receive(packet);
                            String data = new String(buffer, 0, packet.getLength());
                            handleResponse(data, packet.getAddress().getHostAddress(), socket.getLocalAddress().getHostAddress());

                        } catch (SocketTimeoutException e) {
                            // Ignore
                            cont = false;
                        }
                    }
                } while (cont);
            }
        }
        return changeCount != oldChangeCount;
    }

    private void handleRequest(String data, String address, String localAddress) {
        Request request = Request.parse(data);
        if (request == null) {
            trace(String.format("[%s] Invalid request: %s", address, data));
            return;
        }

        if (request.method.equals("NOTIFY")) {
            String usn = request.headers.get("USN");
            if (usn != null || !request.headers.containsKey("NTS")) {
                DeviceInfo device = DeviceInfo.create(request.headers, address, localAddress);
                // alive vs byebye in NTS field?
                String nts = request.headers.get("NTS");
                if (nts.equals("ssdp:alive")) {
                    registerDevice(usn, device);
                } else if (nts.equals("ssdp:byebye")) {
                    ++changeCount;
                    discoveredDevices.remove(usn);
                } else {
                    trace(String.format("[%s] Unsupported NOTIFY response: %s", address, nts));
                }
            } else {
                trace(String.format("[%s] Malformed NOTIFY response: %s", address, data));
            }
        }
        // We ignore M-SEARCH requests
    }

    private void handleResponse(String data, String address, String localAddress) {
        Response response = Response.parse(data);
        if (response == null) {
            trace(String.format("[%s] Invalid response: %s", address, data));
            return;
        }

        if (response.statusCode == 200) {
            String usn = response.headers.get("USN");
            if (usn != null) {
                DeviceInfo device = DeviceInfo.create(response.headers, address, localAddress);
                registerDevice(usn, device);
            } else {
                trace(String.format("[%s] Malformed response: %s", address, data));
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
        try {
            closeConnections();
            log("Stopped successfully");
        } catch (IOException e) {
            log("Stopped with exception: " + e);
        }
    }

    @Override
    public void clearDiscovered() {
        discoveredDevices.clear();
    }

    @Override
    public boolean isConnected() {
        for (Connection c : this.connections) {
            if (!c.mcastSocket.isClosed()) {
                return true;
            }
        }
        return false;
    }
}
