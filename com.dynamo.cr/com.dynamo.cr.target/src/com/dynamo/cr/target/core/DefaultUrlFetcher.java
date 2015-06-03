package com.dynamo.cr.target.core;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.net.URLConnection;

import org.apache.commons.io.IOUtils;


public class DefaultUrlFetcher implements IURLFetcher {

    @Override
    public String fetch(String url) throws IOException {
        int timeout = 2000;
        URLConnection connection = new URL(url).openConnection();
        connection.setRequestProperty("Connection",  "close");
        connection.setConnectTimeout(timeout);
        connection.setReadTimeout(timeout);
        InputStream input = connection.getInputStream();
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        IOUtils.copy(input, output);
        input.close();
        output.close();
        return output.toString("UTF8");
    }

}
