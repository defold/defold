package com.dynamo.cr.editor.fs;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URI;

import javax.ws.rs.core.UriBuilder;

import org.eclipse.core.filesystem.IFileInfo;
import org.eclipse.core.filesystem.IFileStore;
import org.eclipse.core.filesystem.provider.FileInfo;
import org.eclipse.core.filesystem.provider.FileStore;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.Path;

import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo;
import com.dynamo.cr.protocol.proto.Protocol.ResourceType;
import com.sun.jersey.api.client.ClientHandlerException;

public class RepositoryFileStore extends FileStore implements IFileStore {

    private IBranchClient client;
    private Path path;
    private ResourceInfo info_;

    public RepositoryFileStore(IBranchClient client, String path) {
        this.client = client;
        this.path = new Path(path);
        info_ = null;
    }

    private ResourceInfo getInfo() {
        try {
            info_ = client.getResourceInfo(path.toPortableString());
        } catch (RepositoryException e) {
            if (e.getStatusCode() != 404)
                System.err.println(e);
        }
        catch (ClientHandlerException e) {
            System.err.println(e);
        }
        return info_;
    }

    @Override
    public String[] childNames(int options, IProgressMonitor monitor)
            throws CoreException {
        ResourceInfo info = getInfo();
        if (info == null)
            return EMPTY_STRING_ARRAY;
        else {
            String[] ret = new String[info.getSubResourceNamesCount()];
            return info.getSubResourceNamesList().toArray(ret);
        }
    }

    @Override
    public IFileInfo fetchInfo(int options, IProgressMonitor monitor)
            throws CoreException {

        ResourceInfo info = getInfo();

        FileInfo result = new FileInfo();

        if (path.segmentCount() == 0)
            result.setName("/");
        else
            result.setName(path.lastSegment());

        if (info == null) {
            result.setExists(false);
        }
        else {
            result.setExists(true);
            result.setLength(info.getSize());
            result.setLastModified(info.getLastModified());
            result.setDirectory(info.getType() == ResourceType.DIRECTORY);
        }

        //result.setAttribute(EFS.ATTRIBUTE_OWNER_READ | EFS.ATTRIBUTE_OWNER_WRITE, true);
        //result.setLastModified(1);
        return result;
    }

    @Override
    public IFileStore getChild(String name) {
        ResourceInfo info = getInfo();

        if (info == null) {
            return null;
        }
        else {
            IPath new_path = path.append(name);
            return new RepositoryFileStore(client, new_path.toPortableString());
        }
    }

    @Override
    public String getName() {
        String ret = null;
        if (path.segmentCount() == 0)
            ret = "/";
        else
            ret = path.lastSegment();

        return ret;
    }

    @Override
    public IFileStore getParent() {
        if (path.segmentCount() == 0) {
            return null;
        }
        else {
            return new RepositoryFileStore(client, path.removeLastSegments(1).toPortableString());
        }
    }

    @Override
    public InputStream openInputStream(int options, IProgressMonitor monitor)
            throws CoreException {

        byte[] data = new byte[0];
        try {
            data = client.getResourceData(path.toPortableString(), "");
        } catch (RepositoryException e) {
            System.err.println(e);
        }
        return new ByteArrayInputStream(data);
    }

    @Override
    public OutputStream openOutputStream(int options, IProgressMonitor monitor)
            throws CoreException {

        return new RepositoryFileOutputStream(client, path, options, monitor);
    }

    @Override
    public void putInfo(IFileInfo info, int options, IProgressMonitor monitor)
            throws CoreException {
        // TODO: !
    }

    @Override
    public void delete(int options, IProgressMonitor monitor)
            throws CoreException {
        try {
            client.deleteResource(path.toPortableString());
        } catch (RepositoryException e) {
            RepositoryFileSystemPlugin.throwCoreExceptionError(e.getMessage(), e);
        }
    }

    @Override
    public IFileStore mkdir(int options, IProgressMonitor monitor)
            throws CoreException {
        try {
            client.mkdir(path.toPortableString());

            return this;
        } catch (RepositoryException e) {
            RepositoryFileSystemPlugin.throwCoreExceptionError(e.getMessage(), e);
            return null; // Never reached
        }
    }

    @Override
    public void move(IFileStore destination, int options, IProgressMonitor monitor)
            throws CoreException {

        // Let eclipse handle move to another file system (eg local files)
        if (!(destination instanceof RepositoryFileStore)) {
            super.move(destination, options, monitor);
            return;
        }

        try {
            // TODO: Better way of finding the destination path?
            String destination_path = destination.getName();
            IFileStore parent = destination.getParent();
            while (parent != null) {
                if (parent.getParent() != null)
                    destination_path = "/" + destination_path;
                destination_path = parent.getName() + destination_path;
                parent = parent.getParent();
            }
            client.renameResource(path.toPortableString(), destination_path);
        } catch (RepositoryException e) {
            RepositoryFileSystemPlugin.throwCoreExceptionError(e.getMessage(), e);
        }
    }

    @Override
    public URI toURI() {
        URI uri = UriBuilder.fromUri(client.getURI()).scheme("crepo").queryParam("path", path).build();
        return uri;
    }

}
