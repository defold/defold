package com.dynamo.cr.editor.fs.internal;

import java.net.URI;
import java.net.URISyntaxException;
import org.eclipse.core.filesystem.EFS;
import org.eclipse.core.filesystem.IFileStore;
import org.eclipse.core.filesystem.provider.FileSystem;
import org.eclipse.core.runtime.*;
import org.osgi.framework.Bundle;

/**
 * 
 */
public class BundleFileSystem extends FileSystem {

    /**
     * Scheme constant (value "bundle") indicating the bundle file system scheme.
     */
    public static final String SCHEME_BUNDLE = "bundle"; //$NON-NLS-1$

    public IFileStore getStore(URI uri) {
        if (SCHEME_BUNDLE.equals(uri.getScheme())) {
            IPath path = new Path(uri.getPath());
            return new BundleFileStore(Platform.getBundle(uri.getQuery()), path);
        }
        return EFS.getNullFileSystem().getStore(uri);
    }
}