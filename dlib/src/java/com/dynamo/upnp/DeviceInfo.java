package com.dynamo.upnp;

import java.util.Collections;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class DeviceInfo {

    private static Pattern pattern = Pattern.compile(".*?max-age=(\\d+).*");
    public final long expires;
    public final Map<String, String> headers;

    public DeviceInfo(long expires, Map<String, String> headers) {
        this.expires = expires;
        this.headers = Collections.unmodifiableMap(headers);
    }

    public static DeviceInfo create(Map<String, String> headers) {
        String cacheControl = headers.get("CACHE-CONTROL");
        if (cacheControl == null)
            return null;
        Matcher m = pattern.matcher(cacheControl);

        if (!m.matches())
            return null;

        int maxAge = Integer.parseInt(m.group(1));
        long expires = System.currentTimeMillis() + maxAge * 1000;

        return new DeviceInfo(expires, headers);
    }

}
