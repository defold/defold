package com.dynamo.cr.editor.fs.internal;

import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Enumeration;

import org.eclipse.core.filesystem.EFS;
import org.eclipse.core.filesystem.IFileInfo;
import org.eclipse.core.filesystem.IFileStore;
import org.eclipse.core.filesystem.provider.FileInfo;
import org.eclipse.core.filesystem.provider.FileStore;
import org.eclipse.core.internal.filesystem.Messages;
import org.eclipse.core.internal.filesystem.Policy;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.osgi.util.NLS;
import org.osgi.framework.Bundle;

/**
 * File store implementation representing a file or directory inside
 * a zip file.
 */
@SuppressWarnings("restriction")
public class BundleFileStore extends FileStore {
    /**
     * The path of this store within the zip file.
     */
    private IPath path;

    /**
     * The file store that represents the actual zip file.
     */
    private Bundle bundle;

    /**
     * Creates a new zip file store.
     * @param rootStore
     * @param path
     */
    public BundleFileStore(Bundle bundle, IPath path) {
        this.bundle = bundle;
        this.path = path.makeRelative();
    }

    private String[] childEntries(IProgressMonitor monitor) throws CoreException {
        Enumeration<String> paths = bundle.getEntryPaths(this.path.toString());
        ArrayList<String> result = new ArrayList<String>();
        while (paths.hasMoreElements()) {
            String elem = paths.nextElement();
            result.add(elem);
        }
        return result.toArray(new String[result.size()]);
    }

    public IFileInfo[] childInfos(int options, IProgressMonitor monitor) throws CoreException {
        String[] entries = childEntries(monitor);
        int entryCount = entries.length;
        IFileInfo[] infos = new IFileInfo[entryCount];
        for (int i = 0; i < entryCount; i++)
            infos[i] = convertPathToFileInfo(entries[i]);
        return infos;
    }

    public String[] childNames(int options, IProgressMonitor monitor) throws CoreException {
        String[] entries = childEntries(monitor);
        int entryCount = entries.length;
        String[] names = new String[entryCount];
        for (int i = 0; i < entryCount; i++)
            names[i] = computeName(entries[i]);
        return names;
    }

    /**
     * Computes the simple file name for a given zip entry.
     */
    private String computeName(String path) {
        //the entry name is a relative path, with an optional trailing separator
        //We need to strip off the trailing slash, and then take everything after the 
        //last separator as the name
        String name = path;
        int end = name.length() - 1;
        if (name.charAt(end) == '/')
            end--;
        return name.substring(name.lastIndexOf('/', end) + 1, end + 1);
    }

    /**
     * Creates a file info object corresponding to a given zip entry
     * 
     * @param entry the zip entry
     * @return The file info for a zip entry
     */
    private IFileInfo convertPathToFileInfo(String path) {
        FileInfo info = new FileInfo(computeName(path));
        info.setExists(true);
        info.setDirectory(path.endsWith("/"));
        info.setAttribute(EFS.ATTRIBUTE_READ_ONLY, true);
        return info;
    }

    public IFileInfo fetchInfo(int options, IProgressMonitor monitor) throws CoreException {
        return convertPathToFileInfo(path.toString());
    }

    public IFileStore getChild(String name) {
        return new BundleFileStore(bundle, path.append(name));
    }

    public String getName() {
        String name = path.lastSegment();
        return name == null ? "" : name; //$NON-NLS-1$
    }

    public IFileStore getParent() {
        if (path.segmentCount() > 0)
            return new BundleFileStore(bundle, path.removeLastSegments(1));
        //the root entry has no parent
        return null;
    }

    public InputStream openInputStream(int options, IProgressMonitor monitor) throws CoreException {
        try {
            return bundle.getEntry(path.toString()).openStream();
        } catch (IOException e) {
            Policy.error(EFS.ERROR_READ, NLS.bind(Messages.couldNotRead, bundle.toString()), e);
        }
        //can't get here
        return null;
    }

    public URI toURI() {
        try {
            return new URI(BundleFileSystem.SCHEME_BUNDLE, null, path.makeAbsolute().toString(), bundle.getSymbolicName(), null);
        } catch (URISyntaxException e) {
            //should not happen
            throw new RuntimeException(e);
        }
    }
}