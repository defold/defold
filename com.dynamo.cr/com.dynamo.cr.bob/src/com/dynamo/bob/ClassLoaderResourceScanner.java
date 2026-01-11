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

package com.dynamo.bob;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.net.URLDecoder;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.Set;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

import com.dynamo.bob.util.PathUtil;

public class ClassLoaderResourceScanner implements IResourceScanner {

    private URL getURL(String path) {
        return this.getClass().getClassLoader().getResource(path);
    }

    @Override
    public InputStream openInputStream(String path) throws IOException {
        URL url = getURL(path);
        if (url != null) {
            return url.openStream();
        }
        return null;
    }

    @Override
    public boolean exists(String path) {
        return getURL(path) != null;
    }

    @Override
    public boolean isFile(String path) {
        URL url = getURL(path);
        if (url != null) {
            return !url.getFile().isEmpty();
        }
        return false;
    }

    private static void scanDir(File dir, String filter, Set<String> results) {
        File[] files = dir.listFiles();
        for (File file : files) {
            if (PathUtil.wildcardMatch(file.getName(), filter)) {
                results.add(file.getName());
            }
        }
    }

    private static void scanJar(URL resource, String filter, Set<String> results) throws IOException {
        String resPath = resource.getPath();

        // resPath is URL-encoded, e.g. space characters represented as %20, so decode before deriving file paths from it
        String jarPath = URLDecoder.decode(resPath.replaceFirst("[.]jar[!].*", ".jar"), "UTF-8").replaceFirst("file:", "");
        try (JarFile jarFile = new JarFile(jarPath)){
            Enumeration<JarEntry> entries = jarFile.entries();
            while(entries.hasMoreElements()) {
                JarEntry entry = entries.nextElement();
                String entryName = entry.getName();
                if(!entry.isDirectory() && PathUtil.wildcardMatch(entryName, filter)) {
                    results.add(entryName);
                }
            }
        }
    }

    @Override
    public Set<String> scan(String filter) {
        Set<String> results = new HashSet<String>();
        ClassLoader classLoader = this.getClass().getClassLoader();

        String baseDir = filter;
        baseDir = baseDir.substring(0, Math.min(baseDir.lastIndexOf('/'), baseDir.lastIndexOf('*')));
        try {
            Enumeration<URL> e = classLoader.getResources(baseDir);
            while (e.hasMoreElements()) {
                URL url = e.nextElement();
                String proto = url.getProtocol();
                if (proto.equals("file")) {
                    File dir = new File(url.getFile());
                    scanDir(dir, filter, results);
                } else if (proto.equals("jar")) {
                    scanJar(url, filter, results);
                }
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

        return results;
    }

}
