package com.dynamo.bob.test;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import org.apache.commons.io.FileUtils;
import org.junit.rules.ExternalResource;

public class TestLibrariesRule extends ExternalResource {

    private File serverLocation;

    void createLib(String root, String subdir, String name, String sha1) throws IOException {
        File file = new File(String.format("%s/test_lib%s.zip", root, name));
        ZipOutputStream out = new ZipOutputStream(new FileOutputStream(file));
        out.setComment(sha1);
        ZipEntry ze;

        ze = new ZipEntry(String.format("%sgame.project", subdir));
        out.putNextEntry(ze);
        out.write(String.format("[library]\ninclude_dirs=test_lib%s", name).getBytes());

        ze = new ZipEntry(String.format("%stest_lib%s/file%s.in", subdir, name, name));
        out.putNextEntry(ze);
        out.write(String.format("file%s", name).getBytes());

        out.close();
    }

    @Override
    protected void before() throws Throwable {
        serverLocation = new File("server_root");
        serverLocation.mkdirs();
        createLib(serverLocation.getAbsolutePath(), "", "1", "111");
        createLib(serverLocation.getAbsolutePath(), "", "2", "222");
        createLib(serverLocation.getAbsolutePath(), "subdir/", "3", "333");
        createLib(serverLocation.getAbsolutePath(), "subdir/second/", "4", "444");
        createLib(serverLocation.getAbsolutePath(), "", "5", "555");
    }

    @Override
    protected void after() {
        try {
            FileUtils.deleteDirectory(serverLocation);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    public String getServerLocation() {
        return serverLocation.getAbsolutePath();
    }


}
