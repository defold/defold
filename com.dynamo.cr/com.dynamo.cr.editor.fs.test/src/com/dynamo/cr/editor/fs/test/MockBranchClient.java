package com.dynamo.cr.editor.fs.test;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.net.URL;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Map;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Platform;
import org.osgi.framework.Bundle;

import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.IClientFactory;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.CommitDesc;
import com.dynamo.cr.protocol.proto.Protocol.Log;
import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo;
import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo.Builder;
import com.dynamo.cr.protocol.proto.Protocol.ResourceType;

public class MockBranchClient implements IBranchClient {

    Map<String, ResourceInfo> resourceInfos = new HashMap<String, ResourceInfo>();
    private Path tempPath;

    public MockBranchClient(String root) throws IOException {
        Bundle bundle = Platform.getBundle("com.dynamo.cr.editor.fs.test");
        if (bundle == null)
            throw new RuntimeException("Unable to find bundle: com.dynamo.cr.editor.fs.test");

        File tempFile = File.createTempFile("test", "fs");
        tempFile.delete();
        File tempDir = new File(tempFile.getAbsolutePath());
        tempDir.mkdir();
        tempDir.deleteOnExit();
        tempPath = new Path(tempFile.getAbsolutePath());

        Enumeration<URL> entries = bundle.findEntries("/" + root, "*", true);

        while (entries.hasMoreElements()) {
            URL url = entries.nextElement();
            IPath path = new Path(url.getPath()).removeFirstSegments(1);

            File f = new File(tempPath.append(path).toPortableString());
            f.getParentFile().mkdir();
            f.deleteOnExit();
            if (url.getFile().endsWith("/")) {
                f.mkdir();
            } else {
                InputStream is = url.openStream();
                FileOutputStream os = new FileOutputStream(f);
                IOUtils.copy(is, os);
                is.close();
                os.close();
            }
        }
    }

    @Override
    public String getNativeLocation() {
        return null;
    }

    @Override
    public byte[] getResourceData(String path, String revision)
            throws RepositoryException {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public ResourceInfo getResourceInfo(String path) throws RepositoryException {

        File f = new File(tempPath.append(path).toPortableString());
        if (!f.exists()) {
            throw new RepositoryException(String.format("File %s doesn't exists", path), 404);
        }

        Builder b = ResourceInfo.newBuilder();
        b.setName(f.getName())
         .setPath(path)
         .setLastModified(f.lastModified());

        if (f.isDirectory()) {
            b.setSize(0)
             .setType(ResourceType.DIRECTORY);

            for (String sub : f.list()) {
                b.addSubResourceNames(sub);
            }
        }
        else {
            b.setSize((int) f.length())
             .setType(ResourceType.FILE);
        }

        return b.build();
    }

    @Override
    public BranchStatus getBranchStatus(boolean fetch) throws RepositoryException {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public void autoStage() throws RepositoryException {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public void putResourceData(String path, byte[] bytes)
            throws RepositoryException {

        File f = new File(tempPath.append(path).toPortableString());
        if (!f.exists()) {
            throw new RepositoryException(String.format("File %s doesn't exists", path), 404);
        }

        FileOutputStream fos = null;
        try {
            fos = new FileOutputStream(f);
            fos.write(bytes);
            fos.close();
        } catch (IOException e) {
            throw new RepositoryException(e.getMessage(), -1);
        }
        finally {
            if (fos != null) {
                try {
                    fos.close();
                } catch (IOException e) {}
            }
        }
    }

    @Override
    public void mkdir(String path) throws RepositoryException {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public void deleteResource(String path) throws RepositoryException {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public void renameResource(String source, String destination)
            throws RepositoryException {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public void revertResource(String path) throws RepositoryException {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public BranchStatus update() throws RepositoryException {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public CommitDesc commit(String message) throws RepositoryException {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public CommitDesc commitMerge(String message) throws RepositoryException {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public void resolve(String path, String stage) throws RepositoryException {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public void publish() throws RepositoryException {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public IClientFactory getClientFactory() {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public URI getURI() {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public Log log(int maxCount) throws RepositoryException {
        throw new RuntimeException("Not impl.");
    }

    @Override
    public void reset(String mode, String target) throws RepositoryException {
        throw new RuntimeException("Not impl.");
    }

}
