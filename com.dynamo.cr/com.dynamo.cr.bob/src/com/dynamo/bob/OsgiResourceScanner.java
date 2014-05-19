package com.dynamo.bob;

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
    public Set<String> scan(String filter) {
        Set<String> results = new HashSet<String>();
        String filename = FilenameUtils.getName(filter);
        String path = filter.substring(0, filter.length() - filename.length());
        Collection<String> foundResources = bundle.adapt(BundleWiring.class).listResources(path, filename, 0);
        for (String name : foundResources) {
            results.add(name);
        }
        return results;
    }

}
