package com.dynamo.cr.server.openid;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashMap;
import java.util.Map;

import org.apache.commons.io.IOUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class EndPointResolver implements IEndPointResolver {

    private static Map<String, URL> providers = new HashMap<String, URL>();
    private static Logger logger = LoggerFactory.getLogger(EndPointResolver.class);
    private static final int NETWORK_TIMEOUT = 5000;

    static {
        try {
            providers.put("google", new URL("https://www.google.com/accounts/o8/id"));
            providers.put("yahoo", new URL("http://open.login.yahooapis.com/openid20/www.yahoo.com/xrds"));
        } catch (MalformedURLException e) {
            logger.error("Internal error", e);
        }
    }

    @Override
    public EndPoint resolve(String provider) throws IOException, OpenIDException {
        HttpURLConnection connection = (HttpURLConnection) providers.get(provider).openConnection();
        connection.setRequestMethod("GET");
        connection.setConnectTimeout(NETWORK_TIMEOUT);
        connection.setReadTimeout(NETWORK_TIMEOUT);
        connection.connect();
        int code = connection.getResponseCode();
        if (code != 200) {
            throw new OpenIDException(String.format("Unexpected response code %d", code));
        }

        InputStream in = connection.getInputStream();
        ByteArrayOutputStream out = new ByteArrayOutputStream(4 * 1024);
        IOUtils.copy(in, out);
        String content = new String(out.toByteArray(), "UTF-8");

        return EndPoint.create(content, 3600);
    }
}
