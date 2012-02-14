package com.dynamo.bob;

import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.util.Collection;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.Set;

import org.osgi.framework.Bundle;
import org.osgi.framework.wiring.BundleWiring;

/**
 * Scan packages for classes.
 * @author Christian Murray
 *
 */
public class ClassScanner {

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

    /**
     * Scan package for classes
     * @param pkg package to scan
     * @return set of canonical class names
     */
    public static Set<String> scan(String pkg) {
        Set<String> classes = new HashSet<String>();
        ClassLoader classLoader = Thread.currentThread().getContextClassLoader();
        try {
            Enumeration<URL> e = classLoader.getResources(pkg.replace(".", "/"));
            while (e.hasMoreElements()) {
                URL url = e.nextElement();
                String proto = url.getProtocol();
                if (proto.equals("file")) {
                    File dir = new File(url.getFile());
                    scanDir(dir, pkg, classes);
                }
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

        return classes;
    }

    /**
     * OSGi-version of {@link #scan(String)}
     * Scan package in bundle for classes
     * @param pkg package to scan
     * @return set of canonical class names
     */
    public static Set<String> scanBundle(Bundle bundle, String pkg) {
        Set<String> classes = new HashSet<String>();
        Collection<String> foundClasses = bundle.adapt(BundleWiring.class).listResources(pkg.replace(".", "/"), "*.class", 0);
        for (String name : foundClasses) {
            name = name.replace("/", ".").replace("\\", ".");
            name = name.substring(0, name.length() - 6);
            classes.add(name);
        }
        return classes;
    }


}
