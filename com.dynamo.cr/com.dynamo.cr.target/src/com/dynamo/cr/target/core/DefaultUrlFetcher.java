package com.dynamo.cr.target.core;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;

import org.apache.commons.io.IOUtils;


public class DefaultUrlFetcher implements IURLFetcher {

    @Override
    public String fetch(String url) throws IOException {
        InputStream input = new URL(url).openStream();
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        IOUtils.copy(input, output);
        input.close();
        output.close();
        return output.toString("UTF8");
    }

}
