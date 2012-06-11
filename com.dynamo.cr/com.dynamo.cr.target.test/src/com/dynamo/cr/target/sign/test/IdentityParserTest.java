package com.dynamo.cr.target.sign.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import java.io.FileReader;
import java.io.StringWriter;
import java.util.Arrays;
import java.util.HashSet;

import org.apache.commons.io.IOUtils;
import org.junit.Test;

import com.dynamo.cr.target.sign.IdentityParser;

public class IdentityParserTest {

    @Test
    public void testParse() throws Exception {
        FileReader reader = new FileReader("identity_output.txt");
        StringWriter writer = new StringWriter();
        IOUtils.copy(reader, writer);
        String[] list = IdentityParser.parse(writer.toString());
        HashSet<String> set = new HashSet<String>(Arrays.asList(list));
        assertThat(set, is(new HashSet<String>(Arrays.asList("iPhone Distribution: Joe Phone", "iPhone Developer: Joe Phone (ABCDEF123Q)", "gdb-cert"))));
    }

}
