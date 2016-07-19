package com.dynamo.upnp;

import java.util.Collections;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class DeviceInfo {

    private static Pattern pattern = Pattern.compile(".*?max-age=(\\d+).*");
    public final long expires;
    public final Map<String, String> headers;
    public String address;

    public DeviceInfo(long expires, Map<String, String> headers, String address) {
        this.expires = expires;
        this.headers = Collections.unmodifiableMap(headers);
        this.address = address;
    }

    @Override
    public int hashCode() {
        return headers.hashCode() + address.hashCode();
    }

    @Override
    public boolean equals(Object other) {
        if (other instanceof DeviceInfo) {
            DeviceInfo di = (DeviceInfo) other;
            return address.equals(di.address) && headers.equals(di.headers);
        }
        return false;
    }

    public static DeviceInfo create(Map<String, String> headers, String address) {
        int maxAge = 1800;

        String cacheControl = headers.get("CACHE-CONTROL");
        if (cacheControl != null) {
            Matcher m = pattern.matcher(cacheControl);

            if (m.matches()) {
                maxAge = Integer.parseInt(m.group(1));
            }
        }

        long expires = System.currentTimeMillis() + maxAge * 1000;

        return new DeviceInfo(expires, headers, address);
    }

}
