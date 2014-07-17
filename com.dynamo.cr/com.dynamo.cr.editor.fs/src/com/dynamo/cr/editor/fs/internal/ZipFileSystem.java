package com.dynamo.cr.editor.fs.internal;

import java.net.URI;
import java.net.URISyntaxException;
import org.eclipse.core.filesystem.EFS;
import org.eclipse.core.filesystem.IFileStore;
import org.eclipse.core.filesystem.provider.FileSystem;
import org.eclipse.core.runtime.*;

/**
 * 
 */
public class ZipFileSystem extends FileSystem {

    /**
     * Scheme constant (value "zip") indicating the zip file system scheme.
     */
    public static final String SCHEME_ZIP = "zip"; //$NON-NLS-1$

    public IFileStore getStore(URI uri) {
        if (SCHEME_ZIP.equals(uri.getScheme())) {
            IPath path = new Path(uri.getPath());
            try {
                return new ZipFileStore(EFS.getStore(new URI(uri.getQuery())), path);
            } catch (URISyntaxException e) {
                //ignore and fall through below
            } catch (CoreException e) {
                //ignore and fall through below
            }
        }
        return EFS.getNullFileSystem().getStore(uri);
    }
}