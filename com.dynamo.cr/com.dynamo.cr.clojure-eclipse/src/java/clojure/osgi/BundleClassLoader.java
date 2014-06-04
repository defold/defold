/*******************************************************************************
 * Copyright (c) 2009 Laurent Petit and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *    Laurent PETIT - initial API and implementation
 *******************************************************************************/

package clojure.osgi;

import java.net.URL;

import org.osgi.framework.Bundle;

public class BundleClassLoader extends ClassLoader {
    private final Bundle bundle;
    private boolean forceDirect;

    public BundleClassLoader(Bundle bundle) {
        this.bundle = bundle;
    }

    public BundleClassLoader(Bundle bundle, boolean forceDirect) {
        this.bundle = bundle;
        this.forceDirect = forceDirect;
    }

    @Override
    protected Class<?> findClass(String name) throws ClassNotFoundException {
        return bundle.loadClass(name);
    }

    @Override
    public URL getResource(String name) {
        if (forceDirect) {
            return bundle.getEntry(name);
        }
        return bundle.getResource(name);
    }

    @Override
    public String toString() {
        final StringBuilder sb = new StringBuilder();
        sb.append("BundleClassLoader");
        sb.append("[bundle=").append(bundle);
        sb.append(", forceDirect=").append(forceDirect ? "true" : "false");
        sb.append(']');
        return sb.toString();
    }
}