package com.dynamo.cr.editor.core.test;

import static org.junit.Assert.assertEquals;

import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.nio.file.Files;

import javax.net.ssl.HttpsURLConnection;

import org.apache.commons.io.FileUtils;
import org.junit.Test;

public class CustomCacertsTest {
    
    private String copyCacerts(String internalResource) throws IOException {
        File cacertsTmpFile = Files.createTempFile("cacerts", "").toFile();
        cacertsTmpFile.deleteOnExit();
        URL input = getClass().getClassLoader().getResource(internalResource);
        FileUtils.copyURLToFile(input, cacertsTmpFile);
        return cacertsTmpFile.getAbsolutePath();
    }
    
    @Test
    public void testCacerts() throws IOException {
        String cacertsPath = copyCacerts("/cacerts");
        System.setProperty("javax.net.ssl.trustStore", cacertsPath);

        URL url = new URL("https://build.defold.com");
        HttpsURLConnection urlConnection = (HttpsURLConnection) url.openConnection();
        urlConnection.connect();
        assertEquals(200, urlConnection.getResponseCode());
    }
}
