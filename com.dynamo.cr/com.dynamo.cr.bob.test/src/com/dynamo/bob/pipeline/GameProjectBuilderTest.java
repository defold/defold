package com.dynamo.bob.pipeline;

import static org.junit.Assert.*;

import org.junit.Test;

import com.dynamo.bob.Project;

public class GameProjectBuilderTest extends AbstractProtoBuilderTest {

    @Test
    public void testFindResources() throws Exception {
        Project project = new Project();
        ResourceNode rootNode = new ResourceNode("<AnonymousRoot>", "<AnonymousRoot>");
        GameProjectBuilder.findResources(project, rootNode)
        assertEquals(true, true);

        // TODO indentify and implement test for findResource recursive functions in GameProjectBuilder
    }
}
