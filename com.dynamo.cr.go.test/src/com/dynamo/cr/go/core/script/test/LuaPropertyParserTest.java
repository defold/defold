package com.dynamo.cr.go.core.script.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

import org.apache.commons.io.IOUtils;
import org.junit.Test;

import com.dynamo.cr.go.core.script.LuaPropertyParser;
import com.dynamo.cr.go.core.script.LuaPropertyParser.Property;

public class LuaPropertyParserTest {

    String getFile(String file) throws IOException {
        InputStream input = getClass().getClassLoader().getResourceAsStream("com/dynamo/cr/go/core/script/test/" + file);
        ByteArrayOutputStream output = new ByteArrayOutputStream(1024);
        IOUtils.copy(input, output);
        return new String(output.toByteArray());
    }

    @Test
    public void testPropertyParser() throws Exception {
        String str = getFile("test_property_parser1.lua");
        Property[] properties = LuaPropertyParser.parse(str);
        assertThat(properties.length, is(7));
        int i = 0;

        assertThat(properties[i].getStatus(), is(Property.Status.OK));
        assertThat(properties[i].getName(), is("prop1"));
        assertThat(properties[i].getType(), is(Property.Type.NUMBER));
        assertThat(properties[i].getValue(), is("123.456"));
        ++i;

        assertThat(properties[i].getStatus(), is(Property.Status.OK));
        assertThat(properties[i].getName(), is("prop2"));
        assertThat(properties[i].getType(), is(Property.Type.URL));
        assertThat(properties[i].getValue(), is(""));
        ++i;

        assertThat(properties[i].getStatus(), is(Property.Status.OK));
        assertThat(properties[i].getName(), is("prop3"));
        assertThat(properties[i].getType(), is(Property.Type.URL));
        assertThat(properties[i].getValue(), is(""));
        ++i;

        assertThat(properties[i].getStatus(), is(Property.Status.OK));
        assertThat(properties[i].getName(), is("prop4"));
        assertThat(properties[i].getType(), is(Property.Type.HASH));
        assertThat(properties[i].getValue(), is("h1"));
        ++i;

        assertThat(properties[i].getStatus(), is(Property.Status.OK));
        assertThat(properties[i].getName(), is("prop5"));
        assertThat(properties[i].getType(), is(Property.Type.HASH));
        assertThat(properties[i].getValue(), is("h2"));
        ++i;

        assertThat(properties[i].getStatus(), is(Property.Status.PARSE_ERROR));
        ++i;

        assertThat(properties[i].getStatus(), is(Property.Status.PARSE_ERROR));
        ++i;
    }
}

