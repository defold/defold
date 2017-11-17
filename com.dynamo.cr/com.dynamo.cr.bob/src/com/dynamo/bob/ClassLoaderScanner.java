package com.dynamo.bob;

import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.net.URLDecoder;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.Set;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;

import org.apache.commons.io.IOUtils;

public class ClassLoaderScanner implements IClassScanner {

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

    @Override
    public Set<String> scan(String pkg) {
        Set<String> classes = new HashSet<String>();
        ClassLoader classLoader = this.getClass().getClassLoader();

        try {
            Enumeration<URL> e = classLoader.getResources(pkg.replace(".", "/"));
            while (e.hasMoreElements()) {
                URL url = resolveURL(e.nextElement());
                String proto = url.getProtocol();
                if (proto.equals("file")) {
                    File dir = new File(url.getFile());
                    scanDir(dir, pkg, classes);
                } else if (proto.equals("jar")) {
                    scanJar(url, pkg, classes);
                }
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

        return classes;
    }

}
