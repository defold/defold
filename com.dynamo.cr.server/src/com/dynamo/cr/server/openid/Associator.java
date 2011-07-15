package com.dynamo.cr.server.openid;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URLEncoder;

import org.apache.commons.io.IOUtils;

public class Associator implements IAssociator {

    private static final int NETWORK_TIMEOUT = 5000;

    @Override
    public Association associate(EndPoint endPoint) throws IOException, OpenIDException {
        String request = String.format("openid.ns=%s&openid.mode=associate&openid.session_type=no-encryption&openid.assoc_type=HMAC-SHA1", URLEncoder.encode("http://specs.openid.net/auth/2.0", "UTF-8"));
        HttpURLConnection connection = (HttpURLConnection) endPoint.getUrl().openConnection();
        connection.setRequestMethod("POST");
        connection.setDoOutput(true);
        connection.setConnectTimeout(NETWORK_TIMEOUT);
        connection.setReadTimeout(NETWORK_TIMEOUT);

        OutputStream postOut = connection.getOutputStream();
        postOut.write(request.getBytes("UTF-8"));
        postOut.flush();

        connection.connect();
        int code = connection.getResponseCode();

        InputStream in = connection.getInputStream();
        ByteArrayOutputStream out = new ByteArrayOutputStream(4 * 1024);
        IOUtils.copy(in, out);
        String content = new String(out.toByteArray(), "UTF-8");

        if (code != 200) {
            throw new OpenIDException(String.format("Unexpected response code %d", code));
        }

        Association association = Association.create(content);
        return association;
    }

}
