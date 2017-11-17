package com.dynamo.bob;

import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
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

    private URL getURL(String path) {
        return bundle.adapt(BundleWiring.class).getClassLoader().getResource(path);
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
