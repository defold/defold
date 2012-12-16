package com.dynamo.bob;

import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

import org.osgi.framework.Bundle;
import org.osgi.framework.wiring.BundleWiring;

public class OsgiScanner implements IClassScanner {

    private Bundle bundle;

    public OsgiScanner(Bundle bundle) {
        this.bundle = bundle;
    }

    @Override
    public Set<String> scan(String pkg) {
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
