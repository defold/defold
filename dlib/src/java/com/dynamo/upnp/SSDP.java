package com.dynamo.upnp;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.MulticastSocket;
import java.net.SocketTimeoutException;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.StringTokenizer;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.xml.parsers.ParserConfigurationException;
import javax.xml.xpath.XPathExpressionException;

import org.xml.sax.SAXException;

public class SSDP {

    private Logger logger = Logger.getLogger(SSDP.class.getCanonicalName());

    private static final String SSDP_MCAST_ADDR_IP = "239.255.255.250";
    private static final int SSDP_MCAST_PORT = 1900;
    private static final int SSDP_MCAST_TTL = 4;

    private final InetAddress SSDP_MCAST_ADDR;
    private MulticastSocket mcastSocket;
    private DatagramSocket socket;
    private byte[] buffer;
    private Map<String, DeviceInfo> discoveredDevices = new HashMap<String, DeviceInfo>();

    private static final String M_SEARCH_PAYLOAD =
              "M-SEARCH * HTTP/1.1\r\n"
            + "Host: 239.255.255.250:1900\r\n"
            + "MAN: \"ssdp:discover\"\r\n"
            + "MX: 3\r\n"
            + "ST: upnp:rootdevice\r\n\r\n";

    public SSDP() throws IOException {
        buffer = new byte[1500];
        SSDP_MCAST_ADDR = InetAddress.getByName(SSDP_MCAST_ADDR_IP);

        socket = new DatagramSocket();
        socket.setSoTimeout(1);

        mcastSocket = new MulticastSocket(SSDP_MCAST_PORT);
        mcastSocket.joinGroup(SSDP_MCAST_ADDR);
        mcastSocket.setSoTimeout(1);
        mcastSocket.setTimeToLive(SSDP_MCAST_TTL);
    }

    private void sendSearch() throws IOException {
        byte[] buf = M_SEARCH_PAYLOAD.getBytes();
        DatagramPacket p = new DatagramPacket(buf, buf.length, SSDP_MCAST_ADDR,
                SSDP_MCAST_PORT);
        socket.send(p);
    }

    private void expireDiscovered() {
        long now = System.currentTimeMillis();
        Set<String> toExpire = new HashSet<String>();
        for (String id : discoveredDevices.keySet()) {
            DeviceInfo dev = discoveredDevices.get(id);

            if (now >= dev.expires) {
                toExpire.add(id);
            }
        }
        for (String id : toExpire) {
            discoveredDevices.remove(id);
        }
    }

    public void update(boolean search) throws IOException {
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
                handleRequest(data);
            } catch (SocketTimeoutException e) {
                // Ignore
                cont = false;
            }

            try {
                DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                socket.receive(packet);
                String data = new String(buffer, 0, packet.getLength());
                handleResponse(data);
            } catch (SocketTimeoutException e) {
                // Ignore
                cont = false;
            }
        } while (cont);

    }

    private void handleRequest(String data) {
        Request request = Request.parse(data);
        if (request == null) {
            logger.warning("Invalid request: " + data);
            return;
        }

        if (request.method.equals("NOTIFY")) {
            String usn = request.headers.get("USN");
            if (usn != null) {
                discoveredDevices.put(usn, DeviceInfo.create(request.headers));
            }
        }
        // We ignore M-SEARCH requests
    }

    private void handleResponse(String data) {
        Response response = Response.parse(data);
        if (response == null) {
            logger.warning("Invalid response: " + data);
            return;
        }

        if (response.statusCode == 200) {
            String usn = response.headers.get("USN");
            if (usn != null) {
                discoveredDevices.put(usn, DeviceInfo.create(response.headers));
            }
        }
    }

    public DeviceInfo getDeviceInfo(String usn) {
        return discoveredDevices.get(usn);
    }

    public void dispose() {
        this.socket.close();
        this.mcastSocket.close();
    }

    public void clearDiscovered() {
        discoveredDevices.clear();
    }
}
