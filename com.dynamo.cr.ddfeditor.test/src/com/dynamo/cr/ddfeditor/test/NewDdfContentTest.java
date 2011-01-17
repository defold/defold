package com.dynamo.cr.ddfeditor.test;

import java.util.Set;

import org.junit.Test;

import com.dynamo.cr.ddfeditor.ProtoFactory;

public class NewDdfContentTest {

    @Test
    public void test() {
        // We create messages here in order to check that we aren't missing any required fields

        Set<String> extensions = ProtoFactory.getTemplateMessageExtensions();
        for (String extension : extensions) {
            ProtoFactory.getTemplateMessageForExtension(extension);
        }
    }
}
