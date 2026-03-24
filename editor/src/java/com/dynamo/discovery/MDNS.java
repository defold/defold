// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.discovery;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.MulticastSocket;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;

public class MDNS {

    public interface Logger {
        void log(String msg);
    }

    public static final class TestHooks {
        private TestHooks() {
        }

        public static void parsePacket(MDNS mdns, byte[] data, String localAddress, String remoteAddress) {
            mdns.parsePacket(data, data.length, localAddress, remoteAddress);
        }
    }

    private record HostAddress(String address, long expires) {
    }

    private static class ServiceAccumulator {
        String fullName;
        String instanceName;
        String host;
        String address;
        String localAddress;
        int port;
        long ptrExpires;
        long srvExpires;
        long txtExpires;
        boolean hasPtr;
        boolean hasSrv;
        boolean hasTxt;
        Map<String, String> txt = new HashMap<String, String>();
    }

    private static class Connection {
        public final NetworkInterface networkInterface;
        public final String localAddress;
        public MulticastSocket socket;

        Connection(NetworkInterface networkInterface, String localAddress) {
            this.networkInterface = networkInterface;
            this.localAddress = localAddress;
        }

        boolean connect(InetAddress multicastAddress) {
            try {
                socket = new MulticastSocket(MDNS_MCAST_PORT);
                socket.setReuseAddress(true);
                socket.setNetworkInterface(networkInterface);
                socket.joinGroup(new InetSocketAddress(multicastAddress, MDNS_MCAST_PORT), networkInterface);
                socket.setSoTimeout(1);
                socket.setTimeToLive(MDNS_MCAST_TTL);
                return true;
            } catch (IOException e) {
                disconnect(multicastAddress);
                return false;
            }
        }

        void disconnect(InetAddress multicastAddress) {
            if (socket != null) {
                try {
                    socket.leaveGroup(new InetSocketAddress(multicastAddress, MDNS_MCAST_PORT), networkInterface);
                } catch (Exception ignored) {
                    // Ignore socket shutdown races.
                }
                socket.close();
                socket = null;
            }
        }
    }

    public static final int MDNS_MCAST_TTL = 4;
    public static final String MDNS_SERVICE_TYPE = "_defold._tcp.local";

    private static final int MDNS_MCAST_PORT = 5353;
    private static final String MDNS_MCAST_ADDR_IP = "224.0.0.251";

    private static final int DNS_TYPE_A = 1;
    private static final int DNS_TYPE_PTR = 12;
    private static final int DNS_TYPE_TXT = 16;
    private static final int DNS_TYPE_SRV = 33;

    private static final int DNS_CLASS_IN = 1;

    private final Logger logger;
    private final byte[] buffer;

    private InetAddress multicastAddress;
    private List<NetworkInterface> interfaces;
    private final List<Connection> connections;

    private final Map<String, ServiceAccumulator> services;
    private final Map<String, HostAddress> hosts;
    private final Map<String, MDNSServiceInfo> discoveredServices;

    private int changeCount;
    private String defaultLocalAddress;

    public MDNS(Logger logger) {
        this.logger = logger;
        this.buffer = new byte[1500];
        this.connections = new ArrayList<Connection>();
        this.services = new HashMap<String, ServiceAccumulator>();
        this.hosts = new HashMap<String, HostAddress>();
        this.discoveredServices = new HashMap<String, MDNSServiceInfo>();
    }

    private void log(String msg) {
        logger.log("MDNS: " + msg);
    }

    public static List<NetworkInterface> getMCastInterfaces() throws SocketException {
        List<NetworkInterface> interfaces = Collections.list(NetworkInterface.getNetworkInterfaces());
        List<NetworkInterface> result = new ArrayList<NetworkInterface>();

        for (NetworkInterface networkInterface : interfaces) {
            if (networkInterface.isUp()
                    && !networkInterface.isLoopback()
                    && !networkInterface.isPointToPoint()
                    && !networkInterface.isVirtual()
                    && networkInterface.supportsMulticast()) {
                result.add(networkInterface);
            }
        }

        return result;
    }

    public static List<InetAddress> getIPv4Addresses(NetworkInterface networkInterface) throws SocketException {
        List<InetAddress> result = new ArrayList<InetAddress>();
        for (InetAddress address : Collections.list(networkInterface.getInetAddresses())) {
            if (address instanceof Inet4Address) {
                result.add(address);
            }
        }
        return result;
    }

    public boolean setup() throws IOException {
        try {
            multicastAddress = InetAddress.getByName(MDNS_MCAST_ADDR_IP);
            refreshNetworks();
            log("Started successfully");
            return true;
        } catch (UnknownHostException e) {
            log("Fatal error: " + MDNS_MCAST_ADDR_IP);
            return false;
        }
    }

    public boolean update(boolean search) throws IOException {
        int oldChangeCount = changeCount;

        if (search) {
            refreshNetworks();
            sendQuery();
        }

        expireEntries();
        readPackets();
        rebuildDiscovered();

        return changeCount != oldChangeCount;
    }

    public void dispose() {
        closeConnections();
        clearDiscovered();
        services.clear();
        hosts.clear();
        log("Stopped successfully");
    }

    public void clearDiscovered() {
        if (!discoveredServices.isEmpty()) {
            discoveredServices.clear();
            ++changeCount;
        }
    }

    public boolean isConnected() {
        for (Connection connection : connections) {
            if (connection.socket != null && !connection.socket.isClosed()) {
                return true;
            }
        }
        return false;
    }

    public MDNSServiceInfo[] getDevices() {
        return discoveredServices.values().toArray(new MDNSServiceInfo[discoveredServices.size()]);
    }

    private void refreshNetworks() throws IOException {
        List<NetworkInterface> newInterfaces = getMCastInterfaces();
        boolean interfacesChanged = !newInterfaces.equals(interfaces);
        if (interfacesChanged) {
            interfaces = newInterfaces;
            closeConnections();
            services.clear();
            hosts.clear();
            clearDiscovered();

            defaultLocalAddress = null;
        }

        for (NetworkInterface networkInterface : interfaces) {
            List<InetAddress> addresses = getIPv4Addresses(networkInterface);
            if (addresses.isEmpty()) {
                continue;
            }

            String localAddress = addresses.get(0).getHostAddress();
            if (defaultLocalAddress == null) {
                defaultLocalAddress = localAddress;
            }

            Connection connection = findConnection(networkInterface);
            if (connection != null) {
                if (connection.socket != null && !connection.socket.isClosed()) {
                    continue;
                }
                removeConnection(connection);
            }

            Connection newConnection = new Connection(networkInterface, localAddress);
            if (newConnection.connect(multicastAddress)) {
                connections.add(newConnection);
                log(String.format("Connected to multicast network %s: %s", networkInterface.getDisplayName(), localAddress));
            }
        }

        if (connections.isEmpty()) {
            log("Could not find a multicast network");
        }
    }

    private void closeConnections() {
        for (Connection connection : connections) {
            connection.disconnect(multicastAddress);
        }
        connections.clear();
    }

    private Connection findConnection(NetworkInterface networkInterface) {
        for (Connection connection : connections) {
            if (connection.networkInterface.equals(networkInterface)) {
                return connection;
            }
        }
        return null;
    }

    private void removeConnection(Connection connection) {
        if (connection != null) {
            connection.disconnect(multicastAddress);
            connections.remove(connection);
        }
    }

    private static int writeU16(byte[] out, int offset, int value) {
        out[offset] = (byte) ((value >> 8) & 0xff);
        out[offset + 1] = (byte) (value & 0xff);
        return offset + 2;
    }

    private static int readU16(byte[] data, int offset) {
        return ((data[offset] & 0xff) << 8) | (data[offset + 1] & 0xff);
    }

    private static long readU32(byte[] data, int offset) {
        return ((long) (data[offset] & 0xff) << 24)
                | ((long) (data[offset + 1] & 0xff) << 16)
                | ((long) (data[offset + 2] & 0xff) << 8)
                | ((long) (data[offset + 3] & 0xff));
    }

    private static int writeName(byte[] out, int offset, String name) {
        String[] labels = name.split("\\.");
        for (String label : labels) {
            if (label.isEmpty()) {
                continue;
            }
            out[offset++] = (byte) label.length();
            byte[] bytes = label.getBytes();
            System.arraycopy(bytes, 0, out, offset, bytes.length);
            offset += bytes.length;
        }
        out[offset++] = 0;
        return offset;
    }

    private static String readName(byte[] data, int size, int[] offsetRef) {
        StringBuilder name = new StringBuilder();

        int pos = offsetRef[0];
        boolean jumped = false;
        int jumpCount = 0;

        while (true) {
            if (pos >= size) {
                return null;
            }

            int len = data[pos] & 0xff;

            if ((len & 0xC0) == 0xC0) {
                if (pos + 1 >= size) {
                    return null;
                }
                int ptr = ((len & 0x3F) << 8) | (data[pos + 1] & 0xff);
                if (ptr >= size) {
                    return null;
                }
                if (!jumped) {
                    offsetRef[0] = pos + 2;
                    jumped = true;
                }
                pos = ptr;
                if (++jumpCount > 16) {
                    return null;
                }
                continue;
            }

            ++pos;

            if (len == 0) {
                if (!jumped) {
                    offsetRef[0] = pos;
                }
                return name.toString();
            }

            if (len > 63 || pos + len > size) {
                return null;
            }

            if (name.length() > 0) {
                name.append('.');
            }
            for (int i = 0; i < len; ++i) {
                name.append((char) (data[pos + i] & 0xff));
            }
            pos += len;
        }
    }

    private byte[] buildQuery() {
        byte[] out = new byte[512];
        int offset = 0;

        offset = writeU16(out, offset, 0); // id
        offset = writeU16(out, offset, 0); // flags
        offset = writeU16(out, offset, 1); // qdcount
        offset = writeU16(out, offset, 0); // ancount
        offset = writeU16(out, offset, 0); // nscount
        offset = writeU16(out, offset, 0); // arcount

        offset = writeName(out, offset, MDNS_SERVICE_TYPE);
        offset = writeU16(out, offset, DNS_TYPE_PTR);
        offset = writeU16(out, offset, DNS_CLASS_IN);

        byte[] query = new byte[offset];
        System.arraycopy(out, 0, query, 0, offset);
        return query;
    }

    private void sendQuery() {
        byte[] query = buildQuery();

        for (Connection connection : connections) {
            if (connection.socket == null) {
                continue;
            }
            try {
                DatagramPacket packet = new DatagramPacket(query, query.length, multicastAddress, MDNS_MCAST_PORT);
                connection.socket.send(packet);
            } catch (IOException e) {
                log(String.format("Query failed on %s: %s", connection.localAddress, e.getMessage()));
            }
        }
    }

    private void readPackets() {
        for (Connection connection : connections) {
            if (connection.socket == null) {
                continue;
            }

            boolean keepReading = true;
            while (keepReading) {
                try {
                    DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                    connection.socket.receive(packet);
                    String remoteAddress = packet.getAddress() != null ? packet.getAddress().getHostAddress() : null;
                    parsePacket(packet.getData(), packet.getLength(), connection.localAddress, remoteAddress);
                } catch (SocketTimeoutException e) {
                    keepReading = false;
                } catch (IOException e) {
                    log(String.format("Receive failed on %s: %s", connection.localAddress, e.getMessage()));
                    keepReading = false;
                }
            }
        }
    }

    private void parsePacket(byte[] data, int size, String localAddress, String remoteAddress) {
        if (size < 12) {
            return;
        }

        int offset = 0;

        // Header
        offset += 2; // id
        int flags = readU16(data, offset);
        offset += 2;
        int qdCount = readU16(data, offset);
        offset += 2;
        int anCount = readU16(data, offset);
        offset += 2;
        int nsCount = readU16(data, offset);
        offset += 2;
        int arCount = readU16(data, offset);
        offset += 2;

        // Skip questions.
        for (int i = 0; i < qdCount; ++i) {
            int[] nameOffset = new int[] {offset};
            if (readName(data, size, nameOffset) == null) {
                return;
            }
            offset = nameOffset[0] + 4;
            if (offset > size) {
                return;
            }
        }

        int records = anCount + nsCount + arCount;
        for (int i = 0; i < records; ++i) {
            int[] nameOffset = new int[] {offset};
            String name = readName(data, size, nameOffset);
            if (name == null || nameOffset[0] + 10 > size) {
                return;
            }
            offset = nameOffset[0];

            int type = readU16(data, offset);
            offset += 2;
            int klass = readU16(data, offset);
            offset += 2;
            long ttl = readU32(data, offset);
            offset += 4;
            int rdLength = readU16(data, offset);
            offset += 2;

            if (offset + rdLength > size) {
                return;
            }

            parseRecord(data, size, name, type, klass, ttl, rdLength, offset, localAddress, remoteAddress, (flags & 0x8000) != 0);
            offset += rdLength;
        }
    }

    private void parseRecord(byte[] data,
                             int size,
                             String name,
                             int type,
                             int klass,
                             long ttl,
                             int rdLength,
                             int rdataOffset,
                             String localAddress,
                             String remoteAddress,
                             boolean response) {
        voidUnused(klass);
        voidUnused(response);

        long expires = System.currentTimeMillis() + ttl * 1000;

        if (type == DNS_TYPE_PTR && name.equalsIgnoreCase(MDNS_SERVICE_TYPE)) {
            int[] ptrOffset = new int[] {rdataOffset};
            String fullServiceName = readName(data, size, ptrOffset);
            if (fullServiceName == null) {
                return;
            }

            String key = lower(fullServiceName);
            if (ttl == 0) {
                services.remove(key);
                return;
            }

            ServiceAccumulator service = getOrCreateService(key, fullServiceName);
            service.instanceName = instanceName(fullServiceName);
            service.localAddress = localAddress;
            if (response && remoteAddress != null) {
                service.address = remoteAddress;
            }
            service.ptrExpires = expires;
            service.hasPtr = true;
            return;
        }

        if (type == DNS_TYPE_SRV) {
            String key = lower(name);
            if (ttl == 0) {
                ServiceAccumulator service = services.get(key);
                if (service != null) {
                    clearSrv(service);
                }
                return;
            }
            ServiceAccumulator service = getOrCreateService(key, name);

            int pos = rdataOffset;
            if (pos + 6 > size) {
                return;
            }
            pos += 4; // priority + weight
            int port = readU16(data, pos);
            pos += 2;

            int[] hostOffset = new int[] {pos};
            String host = readName(data, size, hostOffset);
            if (host == null) {
                return;
            }

            service.host = host;
            service.port = port;
            service.localAddress = localAddress;
            if (response && remoteAddress != null) {
                service.address = remoteAddress;
            }
            service.srvExpires = expires;
            service.hasSrv = true;
            return;
        }

        if (type == DNS_TYPE_TXT) {
            String key = lower(name);
            if (ttl == 0) {
                ServiceAccumulator service = services.get(key);
                if (service != null) {
                    clearTxt(service);
                }
                return;
            }
            ServiceAccumulator service = getOrCreateService(key, name);

            Map<String, String> txt = parseTxt(data, rdLength, rdataOffset);
            service.txt = txt;
            service.localAddress = localAddress;
            if (response && remoteAddress != null) {
                service.address = remoteAddress;
            }
            service.txtExpires = expires;
            service.hasTxt = true;
            return;
        }

        if (type == DNS_TYPE_A && rdLength == 4) {
            String hostKey = lower(name);
            if (ttl == 0) {
                hosts.remove(hostKey);
                return;
            }

            String address = String.format(Locale.ROOT,
                    "%d.%d.%d.%d",
                    data[rdataOffset] & 0xff,
                    data[rdataOffset + 1] & 0xff,
                    data[rdataOffset + 2] & 0xff,
                    data[rdataOffset + 3] & 0xff);

            hosts.put(hostKey, new HostAddress(address, expires));
        }
    }

    private static void voidUnused(Object ignored) {
        // Explicitly consume vars to silence static analyzers where applicable.
    }

    private static void clearSrv(ServiceAccumulator service) {
        service.host = null;
        service.port = 0;
        service.srvExpires = 0;
        service.hasSrv = false;
    }

    private static void clearTxt(ServiceAccumulator service) {
        service.txt = new HashMap<String, String>();
        service.txtExpires = 0;
        service.hasTxt = false;
    }

    private static long serviceExpires(ServiceAccumulator service) {
        long expires = Math.min(service.ptrExpires, Math.min(service.srvExpires, service.txtExpires));
        return expires > 0 ? expires : 0;
    }

    private ServiceAccumulator getOrCreateService(String key, String fullServiceName) {
        ServiceAccumulator service = services.get(key);
        if (service == null) {
            service = new ServiceAccumulator();
            service.fullName = fullServiceName;
            service.instanceName = instanceName(fullServiceName);
            services.put(key, service);
        }
        return service;
    }

    private static String lower(String value) {
        return value.toLowerCase(Locale.ROOT);
    }

    private static String instanceName(String fullServiceName) {
        String suffix = "." + MDNS_SERVICE_TYPE;
        if (fullServiceName.toLowerCase(Locale.ROOT).endsWith(suffix)) {
            return fullServiceName.substring(0, fullServiceName.length() - suffix.length());
        }
        return fullServiceName;
    }

    private static Map<String, String> parseTxt(byte[] data, int rdLength, int offset) {
        Map<String, String> txt = new HashMap<String, String>();

        int pos = offset;
        int end = offset + rdLength;
        while (pos < end) {
            int len = data[pos] & 0xff;
            ++pos;

            if (len == 0 || pos + len > end) {
                break;
            }

            String entry = new String(data, pos, len);
            int eq = entry.indexOf('=');
            if (eq >= 0) {
                txt.put(entry.substring(0, eq), entry.substring(eq + 1));
            } else {
                txt.put(entry, "");
            }
            pos += len;
        }

        return txt;
    }

    private void expireEntries() {
        long now = System.currentTimeMillis();

        for (Iterator<Map.Entry<String, ServiceAccumulator>> it = services.entrySet().iterator(); it.hasNext();) {
            ServiceAccumulator service = it.next().getValue();
            if (service.hasPtr && now >= service.ptrExpires) {
                it.remove();
                continue;
            }
            if (service.hasSrv && now >= service.srvExpires) {
                clearSrv(service);
            }
            if (service.hasTxt && now >= service.txtExpires) {
                clearTxt(service);
            }
            if (!service.hasPtr && !service.hasSrv && !service.hasTxt) {
                it.remove();
            }
        }

        for (Iterator<Map.Entry<String, HostAddress>> it = hosts.entrySet().iterator(); it.hasNext();) {
            Map.Entry<String, HostAddress> entry = it.next();
            if (now >= entry.getValue().expires()) {
                it.remove();
            }
        }
    }

    private void rebuildDiscovered() {
        Map<String, MDNSServiceInfo> next = new HashMap<String, MDNSServiceInfo>();

        for (ServiceAccumulator service : services.values()) {
            if (!service.hasPtr || !service.hasSrv || !service.hasTxt || service.host == null) {
                continue;
            }

            String id = service.txt.containsKey("id") ? service.txt.get("id") : service.fullName;
            String name = service.txt.containsKey("name") ? service.txt.get("name") : service.instanceName;
            String logPort = service.txt.get("log_port");
            String localAddress = service.localAddress != null ? service.localAddress : defaultLocalAddress;
            String address = service.address;
            long expires = serviceExpires(service);
            if (expires == 0) {
                continue;
            }
            if (address == null) {
                HostAddress hostAddress = hosts.get(lower(service.host));
                if (hostAddress == null) {
                    continue;
                }
                address = hostAddress.address();
                expires = Math.min(expires, hostAddress.expires());
            }

            MDNSServiceInfo info = new MDNSServiceInfo(
                    expires,
                    id,
                    name,
                    service.fullName,
                    service.host,
                    address,
                    localAddress,
                    service.port,
                    logPort,
                    new HashMap<String, String>(service.txt));

            next.put(service.fullName, info);
        }

        if (!next.equals(discoveredServices)) {
            discoveredServices.clear();
            discoveredServices.putAll(next);
            ++changeCount;
        }
    }
}
