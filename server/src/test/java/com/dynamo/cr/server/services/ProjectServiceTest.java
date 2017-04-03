package com.dynamo.cr.server.services;


import org.junit.Test;

import java.util.regex.Pattern;

import static org.junit.Assert.assertTrue;

public class ProjectServiceTest {

    @Test
    public void uploadFilenameTest() {
        ProjectService projectService = new ProjectService();

        Pattern patternFilenameWithExtension = Pattern.compile("[0-9a-f-]+\\.[a-z]+");
        Pattern patternFilenameWithoutExtension = Pattern.compile("[0-9a-f-]+");

        assertTrue(patternFilenameWithExtension.matcher(projectService.createFilename("apa.jpg")).matches());
        assertTrue(patternFilenameWithExtension.matcher(projectService.createFilename("apa.björn.jpg")).matches());

        assertTrue(patternFilenameWithoutExtension.matcher(projectService.createFilename("apa")).matches());
        assertTrue(patternFilenameWithoutExtension.matcher(projectService.createFilename("")).matches());
        assertTrue(patternFilenameWithoutExtension.matcher(projectService.createFilename(null)).matches());
    }

}
