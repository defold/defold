package com.dynamo.bob;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

public class DefaultResource extends AbstractResource<DefaultFileSystem> {

    public DefaultResource(DefaultFileSystem fileSystem, String path) {
        super(fileSystem, path);
    }

    @Override
    public byte[] getContent() throws IOException {
        File f = new File(getAbsPath());
        if (!f.exists())
            return null;

        byte[] buf = new byte[(int) f.length()];
        BufferedInputStream is = new BufferedInputStream(new FileInputStream(f));
        try {
            is.read(buf);
            return buf;
        } finally {
            is.close();
        }
    }

    @Override
    public void setContent(byte[] content) throws IOException {
        File f = new File(getAbsPath());
        if (!f.exists()) {
            String dir = FilenameUtils.getFullPath(getAbsPath());
            File dirFile = new File(dir);
            if (!dirFile.exists()) {
                dirFile.mkdirs();
            }
        }

        BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(f));
        try {
            os.write(content);
        } finally {
            os.close();
        }
    }

    @Override
    public void setContent(InputStream stream) throws IOException {
        File f = new File(getAbsPath());
        if (!f.exists()) {
            String dir = FilenameUtils.getFullPath(getAbsPath());
            File dirFile = new File(dir);
            if (!dirFile.exists()) {
                dirFile.mkdirs();
            }
        }

        try {
            FileUtils.copyInputStreamToFile(stream, f);
        } finally {
            stream.close();
        }
    }

    @Override
    public byte[] sha1() throws IOException {
        return this.fileSystem.sha1(this);
    }

    @Override
    public boolean exists() {
        File file = new File(getAbsPath());
        return file.exists() && file.isFile();
    }

    @Override
    public void remove() {
        new File(getAbsPath()).delete();
    }

}
