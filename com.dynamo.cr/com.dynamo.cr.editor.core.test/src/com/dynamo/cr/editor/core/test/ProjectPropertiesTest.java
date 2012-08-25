package com.dynamo.cr.editor.core.test;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertThat;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import org.junit.Test;

import com.dynamo.cr.editor.core.ProjectProperties;

public class ProjectPropertiesTest {

    ProjectProperties load(String name) throws IOException, ParseException {
        ProjectProperties p = new ProjectProperties();
        p.load(getClass().getResourceAsStream(name));
        return p;
    }

    @Test
    public void testGet() throws Exception {
        ProjectProperties p = load("test.properties");

        assertThat(p.getIntValue("main", "int"), is(123));
        assertThat(p.getStringValue("main", "string"), is("apa"));
        assertThat(p.getBooleanValue("main", "boolean"), is(true));
        assertThat(p.getStringValue("other", "value"), is("foo"));

        assertThat(p.getIntValue("main", "does_not_exists"), nullValue());
        assertThat(p.getStringValue("main", "does_not_exists"), nullValue());
        assertThat(p.getBooleanValue("main", "does_not_exists"), nullValue());
        assertThat(p.getBooleanValue("other", "does_not_exists"), nullValue());
    }

    @Test
    public void testPut() throws Exception {
        ProjectProperties p = new ProjectProperties();
        p.putIntValue("main", "int", 456);
        p.putStringValue("main", "string", "foo");
        p.putBooleanValue("main", "boolean", true);

        assertThat(p.getIntValue("main", "int"), is(456));
        assertThat(p.getStringValue("main", "string"), is("foo"));
        assertThat(p.getBooleanValue("main", "boolean"), is(true));
    }

    @Test
    public void testCreate() throws Exception {
        ProjectProperties p = new ProjectProperties();
        List<String> categoryNames = new ArrayList<String>();
        int value = 0;
        for (int i = 0; i < 20; ++i) {
            String category = Character.toString((char) ('a' + i));
            categoryNames.add(category);

            for (int j = 0; j < 20; ++j) {
                String key = Character.toString((char) ('A' + j));
                p.putIntValue(category, key, value++);
            }
        }

        assertThat(categoryNames, is((List<String>) new ArrayList<String>(p.getCategoryNames())));

        value = 0;
        for (int i = 0; i < 20; ++i) {
            String category = Character.toString((char) ('a' + i));
            for (int j = 0; j < 20; ++j) {
                String key = Character.toString((char) ('A' + j));
                assertThat(p.getIntValue(category, key), is(value++));
            }
        }
    }

    static Map<String, String> allValues(ProjectProperties p) {
        Map<String, String> ret = new LinkedHashMap<String, String>();
        for (String category : p.getCategoryNames()) {
            for (String key : p.getKeys(category)) {
                String value = p.getStringValue(category, key);
                ret.put(String.format("%s.%s", category, key), value);
            }
        }
        return ret;
    }

    @Test
    public void testSaveLoad() throws Exception {
        ProjectProperties p = load("test.properties");
        ByteArrayOutputStream os = new ByteArrayOutputStream();
        p.save(os);

        ProjectProperties pprim = new ProjectProperties();
        pprim.load(new ByteArrayInputStream(os.toByteArray()));

        assertThat(allValues(p), is(allValues(pprim)));
    }

    @Test
    public void testSections() throws Exception {
        ProjectProperties p = load("test.properties");
        assertThat(Arrays.asList("main", "other"), is((List<String>) new ArrayList<String>(p.getCategoryNames())));
    }

}
