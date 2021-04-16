// Copyright 2020 The Defold Foundation
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
import java.net.URL;
import java.net.URLDecoder;
import java.util.List;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.Set;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.lang.ClassLoader;
import java.net.URL;
import java.net.URLClassLoader;

import org.apache.commons.io.IOUtils;

public class ClassLoaderScanner implements IClassScanner {

    private List<URL> extraJars = new ArrayList<>();
    URLClassLoader classLoader = null;
    private boolean dirty = true;

    private static void scanDir(File dir, String packageName, Set<String> classes) {
        File[] files = dir.listFiles();
        for (File file : files) {
            if (file.getName().endsWith(".class")) {
                String name = file.getName();
                name = name.replace("/", ".").replace("\\", ".");
                name = name.substring(0, name.length() - 6);
                classes.add(packageName + "." + name);
            }
        }
    }
    private static void scanJar(URL resource, String pkgname, Set<String> classes) throws IOException {
        String relPath = pkgname.replace('.', '/');
        String resPath = resource.getPath();
        String jarPath = resPath.replaceFirst("[.]jar[!].*", ".jar").replaceFirst("file:", "");
        JarFile jarFile = new JarFile(URLDecoder.decode(jarPath, "UTF-8"));
        try {
            Enumeration<JarEntry> entries = jarFile.entries();
            while(entries.hasMoreElements()) {
                JarEntry entry = entries.nextElement();
                String entryName = entry.getName();
                String className = null;
                if(entryName.endsWith(".class") && entryName.startsWith(relPath) && entryName.length() > (relPath.length() + "/".length())) {
                    className = entryName.replace('/', '.').replace('\\', '.').replace(".class", "");
                    classes.add(className);
                }
            }
        } finally {
            IOUtils.closeQuietly(jarFile);
        }
    }

    protected URL resolveURL(URL url) throws IOException {
        return url;
    }

    // Implemented as a stack
    // The last inserted file takes precedence
    @Override
    public void addUrl(File file) {
        try {
            URL url = file.toURI().toURL();
            extraJars.add(0, url);
            dirty = true;

            System.out.printf("Added plugin: %s\n", file);
        } catch (Exception e) {
            throw new RuntimeException(String.format("Couldn't add '%s' to list of jars", file), e);
        }
    }

    @Override
    public ClassLoader getClassLoader() {
        if (classLoader == null || dirty) {
            try {
                URL[] urls = extraJars.toArray(new URL[0]);
                classLoader = new URLClassLoader(urls, this.getClass().getClassLoader());
            } catch (Exception e) {
                throw new RuntimeException(String.format("Couldn't create custom class loader"), e);
            }
            dirty = false;
        }
        return classLoader;
    }

    private void scanLoader(ClassLoader classLoader, String pkg, Set<String> classes) {
        try {
            Enumeration<URL> e = classLoader.getResources(pkg.replace(".", "/"));
            while (e.hasMoreElements()) {
                URL url = resolveURL(e.nextElement());
                String protocol = url.getProtocol();
                if (protocol.equals("file")) {
                    File dir = new File(url.getFile());
                    scanDir(dir, pkg, classes);
                } else if (protocol.equals("jar")) {
                    scanJar(url, pkg, classes);
                }
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public Set<String> scan(String pkg) {
        Set<String> classes = new HashSet<String>();

        ClassLoader classLoader = getClassLoader();
        scanLoader(classLoader, pkg, classes);

        return classes;
    }
}
