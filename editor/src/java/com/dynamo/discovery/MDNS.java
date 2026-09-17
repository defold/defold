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
import java.nio.charset.StandardCharsets;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

public class MDNS {

    public interface Logger {
        void log(String msg);
    }

    public static final class TestHooks {
        private TestHooks() {
        }

        // Added so parser tests can inject raw packets without relying on multicast sockets.
        public static void parsePacket(MDNS mdns, byte[] data, String localAddress) {
            mdns.parsePacket(data, data.length, localAddress);
        }

        // Added so query tests can assert the exact DNS-SD query bytes produced by the editor implementation.
        public static byte[] buildQuery(MDNS mdns) {
            return mdns.buildQuery();
        }

        // Added so query tests can exercise socket send behavior without going through live multicast discovery.
        public static void sendQuery(MDNS mdns) {
            mdns.sendQuery();
        }

        // Added so pacing tests can drive the browse scheduler without relying on wall-clock sleeps.
        public static void sendQuery(MDNS mdns, long now) {
            mdns.sendQuery(now);
        }

        // Added so pacing tests can assert whether an update would emit a browse query at a given time.
        public static boolean shouldQuery(MDNS mdns, long now) {
            return mdns.shouldQuery(now);
        }

        // Added so packet-read tests can feed deterministic socket data into the private receive loop.
        public static void readPackets(MDNS mdns) {
            mdns.readPackets();
        }

        // Added so packet-read tests can rebuild the public device list after injecting records.
        public static void rebuildDiscovered(MDNS mdns) {
            mdns.rebuildDiscovered();
        }

        // Added so interface-change tests can drive the private network refresh logic directly.
        public static void refreshNetworks(MDNS mdns) throws IOException {
            mdns.refreshNetworks();
        }

        // Added so socket-plumbing tests can inject the multicast destination without calling setup().
        public static void setMulticastAddress(MDNS mdns, InetAddress multicastAddress) {
            mdns.multicastAddress = multicastAddress;
        }

        // Added so interface-change tests can control the previous interface snapshot before refresh().
        public static void setInterfaces(MDNS mdns, List<NetworkInterface> interfaces) {
            mdns.interfaces = interfaces;
        }

        // Added so socket-plumbing tests can attach synthetic sockets to MDNS without exposing Connection publicly.
        public static void addConnection(MDNS mdns, NetworkInterface networkInterface, String localAddress, MulticastSocket socket) {
            Connection connection = new Connection(networkInterface, localAddress);
            connection.socket = socket;
            mdns.connections.add(connection);
        }

        // Added so pacing tests can set the current browse backoff state directly.
        public static void setQuerySchedule(MDNS mdns, long nextQueryAt, long queryIntervalMillis) {
            mdns.nextQueryAt = nextQueryAt;
            mdns.queryIntervalMillis = queryIntervalMillis;
        }

        // Added so pacing tests can force all cached refresh deadlines to the same instant.
        public static void setRefreshAt(MDNS mdns, long refreshAt) {
            for (Map.Entry<String, HostAddress> entry : mdns.hosts.entrySet()) {
                HostAddress host = entry.getValue();
                entry.setValue(new HostAddress(host.address(), host.expires(), refreshAt));
            }

            for (ServiceAccumulator service : mdns.services.values()) {
                service.ptrRefresh = service.hasPtr ? refreshAt : 0;
                service.srvRefresh = service.hasSrv ? refreshAt : 0;
                service.txtRefresh = service.hasTxt ? refreshAt : 0;
            }
        }

        // Added so interface-change tests can assert that refresh() clears stale service accumulators.
        public static int serviceCount(MDNS mdns) {
            return mdns.services.size();
        }

        // Added so interface-change tests can assert that refresh() clears cached host addresses.
        public static int hostCount(MDNS mdns) {
            return mdns.hosts.size();
        }

        // Added so interface-change tests can verify refresh() ignores interface reordering.
        public static boolean sameInterfaces(List<NetworkInterface> left, List<NetworkInterface> right) {
            return MDNS.sameInterfaces(left, right);
        }
    }

    private record HostAddress(String address, long expires, long refreshAt) {
    }

    private record InterfaceSignature(int index, String name, Set<InetAddress> boundAddresses) {
        InterfaceSignature(NetworkInterface networkInterface) {
            this(networkInterface.getIndex(),
                    networkInterface.getName(),
                    new HashSet<InetAddress>(Collections.list(networkInterface.getInetAddresses())));
        }
    }

    private static class ServiceAccumulator {
        String fullName;
        String instanceName;
        String host;
        String localAddress;
        int port;
        long ptrExpires;
        long srvExpires;
        long txtExpires;
        long ptrRefresh;
        long srvRefresh;
        long txtRefresh;
        long ptrTtlMillis;
        long srvTtlMillis;
        long txtTtlMillis;
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

    // RFC 6762 requires multicast DNS traffic to use IP TTL 255.
    public static final int MDNS_MCAST_TTL = 255;
    public static final String MDNS_SERVICE_TYPE = "_defold._tcp.local";

    private static final int MDNS_MCAST_PORT = 5353;
    private static final String MDNS_MCAST_ADDR_IP = "224.0.0.251";
    private static final int MDNS_MAX_PACKET_SIZE = 1500;
    private static final long INITIAL_QUERY_INTERVAL_MS = 1000L;
    private static final long MAX_QUERY_INTERVAL_MS = 60000L;

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
    private long nextQueryAt;
    private long queryIntervalMillis;

    public MDNS(Logger logger) {
        this.logger = logger;
        this.buffer = new byte[MDNS_MAX_PACKET_SIZE];
        this.connections = new ArrayList<Connection>();
        this.services = new HashMap<String, ServiceAccumulator>();
        this.hosts = new HashMap<String, HostAddress>();
        this.discoveredServices = new HashMap<String, MDNSServiceInfo>();
        resetQuerySchedule(0);
    }

    private void log(String msg) {
        logger.log("MDNS: " + msg);
    }

    private void resetQuerySchedule(long nextQueryAt) {
        this.nextQueryAt = nextQueryAt;
        this.queryIntervalMillis = INITIAL_QUERY_INTERVAL_MS;
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

    private static boolean sameInterfaces(List<NetworkInterface> left, List<NetworkInterface> right) {
        if (left == right) {
            return true;
        }

        if (left == null || right == null || left.size() != right.size()) {
            return false;
        }

        Set<InterfaceSignature> leftKeys = new HashSet<InterfaceSignature>();
        for (NetworkInterface networkInterface : left) {
            leftKeys.add(new InterfaceSignature(networkInterface));
        }

        if (leftKeys.size() != left.size()) {
            return false;
        }

        Set<InterfaceSignature> rightKeys = new HashSet<InterfaceSignature>();
        for (NetworkInterface networkInterface : right) {
            rightKeys.add(new InterfaceSignature(networkInterface));
        }

        return rightKeys.size() == right.size() && leftKeys.equals(rightKeys);
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
        long now = System.currentTimeMillis();

        if (search) {
            refreshNetworks();
            if (shouldQuery(now)) {
                sendQuery(now);
            }
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
        boolean interfacesChanged = !sameInterfaces(newInterfaces, interfaces);
        if (interfacesChanged) {
            interfaces = newInterfaces;
            closeConnections();
            services.clear();
            hosts.clear();
            clearDiscovered();

            defaultLocalAddress = null;
            resetQuerySchedule(0);
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
                resetQuerySchedule(0);
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

    private static int writeU32(byte[] out, int offset, long value) {
        out[offset] = (byte) ((value >> 24) & 0xff);
        out[offset + 1] = (byte) ((value >> 16) & 0xff);
        out[offset + 2] = (byte) ((value >> 8) & 0xff);
        out[offset + 3] = (byte) (value & 0xff);
        return offset + 4;
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

    private static boolean isDigit(char c) {
        return c >= '0' && c <= '9';
    }

    private static char decodeEscapedChar(String value, int[] indexRef) {
        int index = indexRef[0];
        char c = value.charAt(index++);
        if (c != '\\') {
            indexRef[0] = index;
            return c;
        }

        if (index >= value.length()) {
            indexRef[0] = value.length();
            return '\\';
        }

        if (index + 2 < value.length()
                && isDigit(value.charAt(index))
                && isDigit(value.charAt(index + 1))
                && isDigit(value.charAt(index + 2))) {
            char decoded = (char) (((value.charAt(index) - '0') * 100)
                    + ((value.charAt(index + 1) - '0') * 10)
                    + (value.charAt(index + 2) - '0'));
            indexRef[0] = index + 3;
            return decoded;
        }

        char decoded = value.charAt(index);
        indexRef[0] = index + 1;
        return decoded;
    }

    private static String escapeDnsText(String value) {
        if (value == null) {
            return "";
        }

        StringBuilder escaped = new StringBuilder(value.length() + 8);
        for (int i = 0; i < value.length(); ++i) {
            char c = value.charAt(i);
            if (c == '.' || c == '\\') {
                escaped.append('\\');
            }
            escaped.append(c);
        }
        return escaped.toString();
    }

    private static String unescapeDnsText(String value) {
        if (value == null) {
            return "";
        }

        StringBuilder unescaped = new StringBuilder(value.length());
        int[] indexRef = new int[] {0};
        while (indexRef[0] < value.length()) {
            unescaped.append(decodeEscapedChar(value, indexRef));
        }
        return unescaped.toString();
    }

    private static List<String> splitDnsLabels(String name) {
        List<String> labels = new ArrayList<String>();
        StringBuilder current = new StringBuilder();
        int[] indexRef = new int[] {0};
        while (indexRef[0] < name.length()) {
            char c = name.charAt(indexRef[0]);
            if (c == '.') {
                labels.add(current.toString());
                current.setLength(0);
                ++indexRef[0];
                continue;
            }

            current.append(decodeEscapedChar(name, indexRef));
        }

        if (current.length() > 0 || name.endsWith(".")) {
            labels.add(current.toString());
        }
        return labels;
    }

    private static int writeName(byte[] out, int offset, String name) {
        for (String label : splitDnsLabels(name)) {
            if (label.isEmpty()) {
                continue;
            }
            byte[] bytes = label.getBytes(StandardCharsets.UTF_8);
            out[offset++] = (byte) bytes.length;
            System.arraycopy(bytes, 0, out, offset, bytes.length);
            offset += bytes.length;
        }
        out[offset++] = 0;
        return offset;
    }

    private static int nameWireLength(String name) {
        int length = 1; // zero terminator
        for (String label : splitDnsLabels(name)) {
            if (label.isEmpty()) {
                continue;
            }
            length += 1 + label.getBytes(StandardCharsets.UTF_8).length;
        }
        return length;
    }

    private static byte[] buildPtrRecord(String ownerName, String fullServiceName, int ttl) {
        int ownerNameLength = nameWireLength(ownerName);
        int fullServiceNameLength = nameWireLength(fullServiceName);
        byte[] record = new byte[ownerNameLength + 10 + fullServiceNameLength];
        int offset = 0;
        offset = writeName(record, offset, ownerName);
        offset = writeU16(record, offset, DNS_TYPE_PTR);
        offset = writeU16(record, offset, DNS_CLASS_IN);
        offset = (int) writeU32(record, offset, ttl);
        offset = writeU16(record, offset, fullServiceNameLength);
        writeName(record, offset, fullServiceName);
        return record;
    }

    private static int remainingTtlSeconds(long expires, long now) {
        if (expires <= now) {
            return 0;
        }
        return (int) ((expires - now + 999L) / 1000L);
    }

    private static boolean shouldIncludeKnownAnswer(ServiceAccumulator service, long now) {
        if (!service.hasPtr || service.ptrExpires <= now || service.ptrTtlMillis <= 0) {
            return false;
        }

        long remaining = service.ptrExpires - now;
        return remaining > service.ptrTtlMillis / 2;
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

            String label = new String(data, pos, len, StandardCharsets.UTF_8);
            for (int i = 0; i < label.length(); ++i) {
                char c = label.charAt(i);
                if (c == '.' || c == '\\') {
                    name.append('\\');
                }
                name.append(c);
            }
            pos += len;
        }
    }

    private byte[] buildQuery() {
        return buildQuery(System.currentTimeMillis());
    }

    private byte[] buildQuery(long now) {
        byte[] out = new byte[MDNS_MAX_PACKET_SIZE];
        int offset = 0;
        int answerCount = 0;

        offset = writeU16(out, offset, 0); // id
        offset = writeU16(out, offset, 0); // flags
        offset = writeU16(out, offset, 1); // qdcount
        offset = writeU16(out, offset, 0); // ancount (patched later)
        offset = writeU16(out, offset, 0); // nscount
        offset = writeU16(out, offset, 0); // arcount

        offset = writeName(out, offset, MDNS_SERVICE_TYPE);
        offset = writeU16(out, offset, DNS_TYPE_PTR);
        offset = writeU16(out, offset, DNS_CLASS_IN);

        for (ServiceAccumulator service : services.values()) {
            if (!shouldIncludeKnownAnswer(service, now)) {
                continue;
            }

            int ttl = remainingTtlSeconds(service.ptrExpires, now);
            if (ttl == 0) {
                continue;
            }
            byte[] knownAnswer = buildPtrRecord(MDNS_SERVICE_TYPE, service.fullName, ttl);
            // Keep the question valid and drop overflow answers once the
            // current UDP packet budget is exhausted instead of splitting.
            if (offset + knownAnswer.length > out.length) {
                continue;
            }
            System.arraycopy(knownAnswer, 0, out, offset, knownAnswer.length);
            offset += knownAnswer.length;
            ++answerCount;
        }

        out[6] = (byte) ((answerCount >> 8) & 0xff);
        out[7] = (byte) (answerCount & 0xff);

        byte[] query = new byte[offset];
        System.arraycopy(out, 0, query, 0, offset);
        return query;
    }

    private void sendQuery() {
        sendQuery(System.currentTimeMillis());
    }

    private boolean shouldQuery(long now) {
        // Query either when the exponential backoff fires or when a cached
        // record reaches its half-TTL refresh point, whichever comes first.
        long refreshAt = nextRefreshAt();
        long due = nextQueryAt == 0 ? refreshAt
                : refreshAt == 0 ? nextQueryAt
                : Math.min(nextQueryAt, refreshAt);
        return due == 0 || now >= due;
    }

    private void sendQuery(long now) {
        byte[] query = buildQuery(now);

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

        long retryAt = now + queryIntervalMillis;
        nextQueryAt = retryAt;
        rescheduleRefreshes(now, retryAt);
        if (queryIntervalMillis < MAX_QUERY_INTERVAL_MS) {
            queryIntervalMillis = Math.min(queryIntervalMillis * 3L, MAX_QUERY_INTERVAL_MS);
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
                    parsePacket(packet.getData(), packet.getLength(), connection.localAddress);
                } catch (SocketTimeoutException e) {
                    keepReading = false;
                } catch (IOException e) {
                    log(String.format("Receive failed on %s: %s", connection.localAddress, e.getMessage()));
                    keepReading = false;
                }
            }
        }
    }

    private void parsePacket(byte[] data, int size, String localAddress) {
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

        if ((flags & 0x8000) == 0) {
            return;
        }

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
            offset += 2; // class
            long ttl = readU32(data, offset);
            offset += 4;
            int rdLength = readU16(data, offset);
            offset += 2;

            if (offset + rdLength > size) {
                return;
            }

            parseRecord(data, size, name, type, ttl, rdLength, offset, localAddress);
            offset += rdLength;
        }
    }

    private void parseRecord(byte[] data,
                             int size,
                             String name,
                             int type,
                             long ttl,
                             int rdLength,
                             int rdataOffset,
                             String localAddress) {
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
            service.ptrExpires = expires;
            service.ptrRefresh = System.currentTimeMillis() + Math.max(1L, ttl * 500L);
            service.ptrTtlMillis = ttl * 1000L;
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
            service.srvExpires = expires;
            service.srvRefresh = System.currentTimeMillis() + Math.max(1L, ttl * 500L);
            service.srvTtlMillis = ttl * 1000L;
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
            service.txtExpires = expires;
            service.txtRefresh = System.currentTimeMillis() + Math.max(1L, ttl * 500L);
            service.txtTtlMillis = ttl * 1000L;
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

            hosts.put(hostKey, new HostAddress(address, expires, System.currentTimeMillis() + Math.max(1L, ttl * 500L)));
        }
    }

    private static void clearSrv(ServiceAccumulator service) {
        service.host = null;
        service.port = 0;
        service.srvExpires = 0;
        service.srvRefresh = 0;
        service.srvTtlMillis = 0;
        service.hasSrv = false;
    }

    private static void clearTxt(ServiceAccumulator service) {
        service.txt = new HashMap<String, String>();
        service.txtExpires = 0;
        service.txtRefresh = 0;
        service.txtTtlMillis = 0;
        service.hasTxt = false;
    }

    private static long serviceExpires(ServiceAccumulator service) {
        long expires = Math.min(service.ptrExpires, Math.min(service.srvExpires, service.txtExpires));
        return expires > 0 ? expires : 0;
    }

    private HostAddress resolveServiceAddress(ServiceAccumulator service) {
        return service.host != null ? hosts.get(lower(service.host)) : null;
    }

    private long nextRefreshAt() {
        long next = 0;

        for (HostAddress host : hosts.values()) {
            if (host.refreshAt() > 0 && (next == 0 || host.refreshAt() < next)) {
                next = host.refreshAt();
            }
        }

        for (ServiceAccumulator service : services.values()) {
            if (service.ptrRefresh > 0 && (next == 0 || service.ptrRefresh < next)) {
                next = service.ptrRefresh;
            }
            if (service.srvRefresh > 0 && (next == 0 || service.srvRefresh < next)) {
                next = service.srvRefresh;
            }
            if (service.txtRefresh > 0 && (next == 0 || service.txtRefresh < next)) {
                next = service.txtRefresh;
            }
        }

        return next;
    }

    private static long rescheduleRefreshAt(long refreshAt, long expires, long now, long retryAt) {
        if (refreshAt == 0 || refreshAt > now) {
            return refreshAt;
        }

        return expires > 0 ? Math.min(expires, retryAt) : retryAt;
    }

    private void rescheduleRefreshes(long now, long retryAt) {
        // Once a half-TTL refresh fires, keep retrying on the normal browse
        // backoff until a fresh response arrives. Leaving the stale refresh
        // deadline overdue would make shouldQuery() fire on every update().
        for (Map.Entry<String, HostAddress> entry : hosts.entrySet()) {
            HostAddress host = entry.getValue();
            long refreshAt = rescheduleRefreshAt(host.refreshAt(), host.expires(), now, retryAt);
            if (refreshAt != host.refreshAt()) {
                entry.setValue(new HostAddress(host.address(), host.expires(), refreshAt));
            }
        }

        for (ServiceAccumulator service : services.values()) {
            service.ptrRefresh = rescheduleRefreshAt(service.ptrRefresh, service.ptrExpires, now, retryAt);
            service.srvRefresh = rescheduleRefreshAt(service.srvRefresh, service.srvExpires, now, retryAt);
            service.txtRefresh = rescheduleRefreshAt(service.txtRefresh, service.txtExpires, now, retryAt);
        }
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
            return unescapeDnsText(fullServiceName.substring(0, fullServiceName.length() - suffix.length()));
        }
        return unescapeDnsText(fullServiceName);
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

            String entry = new String(data, pos, len, StandardCharsets.UTF_8);
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
            long expires = serviceExpires(service);
            if (expires == 0) {
                continue;
            }
            HostAddress hostAddress = resolveServiceAddress(service);
            if (hostAddress == null) {
                continue;
            }
            String address = hostAddress.address();
            expires = Math.min(expires, hostAddress.expires());

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
