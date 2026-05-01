// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.fs;

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

    public void appendContent(byte[] content) throws IOException {
        File f = new File(getAbsPath());
        if (!f.exists()) {
            String dir = FilenameUtils.getFullPath(getAbsPath());
            File dirFile = new File(dir);
            if (!dirFile.exists()) {
                dirFile.mkdirs();
            }
        }

        BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(f, true));
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

    @Override
    public long getLastModified() {
        return new File(getAbsPath()).lastModified();
    }

    @Override
    public boolean isFile() {
        File f = new File(getAbsPath());
        return f.isFile();
    }

}
