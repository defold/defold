package com.dynamo.cr.server.archive.test;

import com.dynamo.cr.server.git.archive.ZipUtil;
import org.junit.Test;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

import static org.junit.Assert.assertTrue;

public class ZipUtilTest {

    @Test
    public void zipFileShouldBeCreated() throws IOException {
        Path sourceDirectory = Files.createTempDirectory("ZipUtilTest");
        Files.createTempFile(sourceDirectory, "tmp", "txt");

        Path target = Files.createTempFile("target", ".zip");
        assertTrue(target.toFile().delete());

        ZipUtil.zipFilesAtomic(sourceDirectory.toFile(), target.toFile(), "");
        assertTrue(Files.exists(target));
    }
}
