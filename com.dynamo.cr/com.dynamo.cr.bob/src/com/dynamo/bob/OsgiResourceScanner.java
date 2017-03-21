package com.dynamo.bob;

import java.io.IOException;
import java.io.InputStream;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

import org.apache.commons.io.FilenameUtils;
import org.osgi.framework.Bundle;
import org.osgi.framework.wiring.BundleWiring;

public class OsgiResourceScanner implements IResourceScanner {

    private Bundle bundle;

    public OsgiResourceScanner(Bundle bundle) {
        this.bundle = bundle;
    }

    @Override
    public InputStream openInputStream(String path) throws IOException {
        return bundle.adapt(BundleWiring.class).getClassLoader().getResource(path).openStream();
    }

    @Override
    public boolean exists(String path) {
        return bundle.adapt(BundleWiring.class).getClassLoader().getResource(path) != null;
    }

    @Override
    public boolean isFile(String path) {
        return !bundle.adapt(BundleWiring.class).getClassLoader().getResource(path).getFile().isEmpty();
    }

    private void scanDir(String path, String filter, Set<String> result) {
        Collection<String> foundResources = bundle.adapt(BundleWiring.class).listResources(path, filter, 0);
        for (String name : foundResources) {
            result.add(name);
            if (name.endsWith("/")) {
                scanDir(name, "*", result);
            }
        }
    }

    @Override
    public Set<String> scan(String filter) {
        Set<String> results = new HashSet<String>();
        String filename = FilenameUtils.getName(filter);
        String path = filter.substring(0, filter.length() - filename.length());
        scanDir(path, filename, results);
        return results;
    }

}
