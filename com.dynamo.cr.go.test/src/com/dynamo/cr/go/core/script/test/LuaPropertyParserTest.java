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
        assertThat(properties.length, is(5));

        assertThat(properties[0].getStatus(), is(Property.Status.OK));
        assertThat(properties[0].getName(), is("prop1"));
        assertThat(properties[0].getType(), is(Property.Type.NUMBER));
        assertThat(properties[0].getValue(), is("123.456"));

        assertThat(properties[1].getStatus(), is(Property.Status.OK));
        assertThat(properties[1].getName(), is("prop2"));
        assertThat(properties[1].getType(), is(Property.Type.URL));
        assertThat(properties[1].getValue(), is("go.url ()"));

        assertThat(properties[2].getStatus(), is(Property.Status.OK));
        assertThat(properties[2].getName(), is("prop3"));
        assertThat(properties[2].getType(), is(Property.Type.URL));
        assertThat(properties[2].getValue(), is("go.url( )"));

        assertThat(properties[3].getStatus(), is(Property.Status.PARSE_ERROR));
        assertThat(properties[4].getStatus(), is(Property.Status.PARSE_ERROR));
    }
}
