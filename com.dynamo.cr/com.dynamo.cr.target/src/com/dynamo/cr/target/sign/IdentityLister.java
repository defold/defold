package com.dynamo.cr.target.sign;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

import org.apache.commons.io.IOUtils;

public class IdentityLister implements IIdentityLister {

    @Override
    public String[] listIdentities() throws IOException {

        Process process = new ProcessBuilder("security", "find-identity", "-v", "-p", "codesigning").start();
        InputStream input = process.getInputStream();
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        IOUtils.copy(input, output);
        String[] list = IdentityParser.parse(new String(output.toByteArray(), "UTF8"));
        return list;
    }

}
