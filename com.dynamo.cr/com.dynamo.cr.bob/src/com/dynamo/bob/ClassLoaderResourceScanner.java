package com.dynamo.bob;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.Set;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

import org.apache.commons.io.IOUtils;

import com.dynamo.bob.util.PathUtil;

public class ClassLoaderResourceScanner implements IResourceScanner {

    @Override
    public InputStream openInputStream(String path) throws IOException {
        return this.getClass().getClassLoader().getResource(path).openStream();
    }

    @Override
    public boolean exists(String path) {
        return this.getClass().getClassLoader().getResource(path) != null;
    }

    @Override
    public boolean isFile(String path) {
        return !this.getClass().getClassLoader().getResource(path).getFile().isEmpty();
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
        String jarPath = resPath.replaceFirst("[.]jar[!].*", ".jar").replaceFirst("file:", "");
        JarFile jarFile = null;
        try {
            jarFile = new JarFile(jarPath);
            Enumeration<JarEntry> entries = jarFile.entries();
            while(entries.hasMoreElements()) {
                JarEntry entry = entries.nextElement();
                String entryName = entry.getName();
                if(!entry.isDirectory() && PathUtil.wildcardMatch(entryName, filter)) {
                    results.add(entryName);
                }
            }
        } finally {
            if (jarFile != null) {
                IOUtils.closeQuietly(jarFile);
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
