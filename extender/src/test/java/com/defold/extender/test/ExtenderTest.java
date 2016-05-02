package com.defold.extender.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;

import org.apache.commons.io.FileUtils;
import org.apache.http.HttpEntity;
import org.apache.http.HttpResponse;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.mime.MultipartEntityBuilder;
import org.apache.http.impl.client.CloseableHttpClient;
import org.apache.http.impl.client.HttpClientBuilder;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.context.embedded.EmbeddedWebApplicationContext;
import org.springframework.boot.test.SpringApplicationConfiguration;
import org.springframework.boot.test.WebIntegrationTest;
import org.springframework.test.context.junit4.SpringJUnit4ClassRunner;
import org.yaml.snakeyaml.Yaml;

import com.defold.extender.Configuration;
import com.defold.extender.Extender;
import com.defold.extender.ExtenderApplication;

@RunWith(SpringJUnit4ClassRunner.class)
@SpringApplicationConfiguration(ExtenderApplication.class)
@WebIntegrationTest(randomPort = true)
public class ExtenderTest {

    @Autowired
    EmbeddedWebApplicationContext server;

    @Value("${local.server.port}")
    int port;

    @Test
    public void testBuild() throws IOException, InterruptedException {

        Yaml yaml = new Yaml();
        Configuration config = yaml.loadAs(FileUtils.readFileToString(new File("test-data/config.yml")), Configuration.class);
        //System.out.println(yaml.dumpAs(config, Tag.MAP, null));
        //System.out.println("-----------");

        Extender ext = new Extender(config, "x86-osx", new File("test-data/ext"));
        File engine = ext.buildEngine();
        assertTrue(engine.isFile());
        System.out.println(engine);
        ext.dispose();
    }

    @Test
    public void testRemoteBuild() throws ClientProtocolException, IOException {
        MultipartEntityBuilder builder = MultipartEntityBuilder.create();
        File root = new File("test-data/ext");
        for (String s : new String[] {"ext.manifest", "src/test_ext.cpp", "include/test_ext.h"}) {
            builder.addBinaryBody(s, new File(root, s));
        }

        HttpEntity entity = builder.build();
        String url = String.format("http://localhost:%d/build/x86-osx", port);
        HttpPost request = new HttpPost(url);

        request.setEntity(entity);

        CloseableHttpClient client = HttpClientBuilder.create().build();
        HttpResponse response = client.execute(request);
        assertEquals(200, response.getStatusLine().getStatusCode());
        /*
        try (InputStream is = response.getEntity().getContent();) {
            FileUtils.copyInputStreamToFile(is, new File("/tmp/hej"));
        }*/

        client.close();
    }

}
