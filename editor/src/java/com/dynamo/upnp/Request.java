package com.dynamo.upnp;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.StringTokenizer;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class Request {

    public final String method;
    public final String resource;
    public final int major;
    public final int minor;
    public final Map<String, String> headers;

    private final static Pattern pattern = Pattern.compile("(.*?) (.*?) HTTP/([0-9])\\.([0-9])");

    public Request(String method, String resource, int major, int minor, Map<String, String> headers) {
        this.method = method;
        this.resource = resource;
        this.major = major;
        this.minor = minor;
        this.headers = Collections.unmodifiableMap(headers);
    }

    public static Request parse(String header) {
        StringTokenizer st = new StringTokenizer(header, "\r\n");
        Map<String, String> headers = new HashMap<String, String>();
        int i = 0;
        String method = null;
        String resource = null;
        int major = -1;
        int minor = -1;

        while (st.hasMoreTokens()) {
            String token = st.nextToken();

            if (i == 0) {
                Matcher m = pattern.matcher(token);
                if (!m.matches()) {
                    return null;
                }
                method = m.group(1);
                resource = m.group(2);
                major = Integer.parseInt(m.group(3));
                minor = Integer.parseInt(m.group(4));

            } else {
                int index = token.indexOf(":");
                if (index != -1) {
                    String key = token.substring(0, index).toUpperCase();
                    String value = token.substring(index+1).trim();
                    headers.put(key, value);
                }
            }
            ++i;
        }

        return new Request(method, resource, major, minor, headers);
    }

    @Override
    public String toString() {
        return String.format("%s %s HTTP/%d.%d", method, resource, major, minor);
    }

}
