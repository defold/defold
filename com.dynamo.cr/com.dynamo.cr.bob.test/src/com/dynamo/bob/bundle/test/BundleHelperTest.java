package com.dynamo.bob.bundle.test;

import static org.junit.Assert.assertEquals;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.text.ParseException;
import java.util.Map;

import org.junit.Test;

import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.util.BobProjectProperties;

public class BundleHelperTest {

    void testValue(String p, String c, String k, Object expected) throws IOException, ParseException {
        BobProjectProperties properties = new BobProjectProperties();
        properties.load(new ByteArrayInputStream(p.getBytes()));
        Map<String, Map<String, Object>> map = BundleHelper.createPropertiesMap(properties);

        Object actual = map.get(c).get(k);
        assertEquals(expected, actual);
    }

    @Test
    public void testProperties() throws IOException, ParseException {
        testValue("[project]\nwrite_log=0", "project", "write_log", false);
        testValue("[project]\nwrite_log=1", "project", "write_log", true);
        testValue("", "project", "write_log", false);
        testValue("", "project", "title", "unnamed");
    }
}
