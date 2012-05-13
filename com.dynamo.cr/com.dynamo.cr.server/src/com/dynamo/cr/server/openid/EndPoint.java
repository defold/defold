package com.dynamo.cr.server.openid;

import java.net.MalformedURLException;
import java.net.URL;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class EndPoint {
    private URL url;
    // NOTE: The value is in ms to match System#currentTimeMillis
    private long expires;

    private static Pattern uriPattern = Pattern.compile(".*?<URI>(.*)</URI>.*", Pattern.DOTALL);;

    public EndPoint(URL url, long expires_in) {
        this.url = url;
        this.expires = System.currentTimeMillis() + expires_in * 1000;
    }

    public URL getUrl() {
        return url;
    }

    public boolean isExpired() {
        return expires <= System.currentTimeMillis();
    }

    @Override
    public String toString() {
        return String.format("EndPoint(url=%s, expires=%d)", url.toString(), expires);
    }

    public static EndPoint create(String content, long expires_in) throws MalformedURLException, OpenIDException {
        Matcher matcher = uriPattern.matcher(content);
        if (matcher.matches()) {
            EndPoint ep = new EndPoint(new URL(matcher.group(1)), expires_in);
            return ep;
        }
        else {
            throw new OpenIDException("Invalid endpoint response from OpenID provider");
        }
    }
}