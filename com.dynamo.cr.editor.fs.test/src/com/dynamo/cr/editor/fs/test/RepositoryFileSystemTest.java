package com.dynamo.cr.editor.fs.test;

import static org.junit.Assert.assertEquals;

import java.io.IOException;
import java.net.URI;

import javax.ws.rs.core.UriBuilder;

import org.eclipse.core.filesystem.EFS;
import org.eclipse.core.filesystem.IFileInfo;
import org.eclipse.core.filesystem.IFileStore;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.editor.fs.RepositoryFileSystemPlugin;
import com.dynamo.cr.editor.fs.internal.ResourceInfoCache;
import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo;
import com.dynamo.cr.protocol.proto.Protocol.ResourceType;

public class RepositoryFileSystemTest {

    @Before
    public void setUp() throws IOException {
        MockBranchClient branchClient = new MockBranchClient("test");
        RepositoryFileSystemPlugin.setClientFactory(new MockClientFactory(branchClient));
    }

    static URI makeURI(String path) {
        return UriBuilder.fromPath("/").scheme("crepo").queryParam("path", path).build();
    }

    @Test
    public void testBasic() throws Exception {
        IFileStore f1 = EFS.getStore(makeURI("/a/f1.txt"));
        IFileInfo f1Info = f1.fetchInfo();
        assertEquals(true, f1Info.exists());
        assertEquals("f1.txt", f1Info.getName());
    }

    @Test
    public void testDoesNotExists() throws Exception {
        IFileStore store = EFS.getStore(makeURI("/a/doesNotExists.txt"));
        IFileInfo info = store.fetchInfo();
        assertEquals(false, info.exists());
    }

    @Test
    public void testBasic__() throws Exception {
        ResourceInfoCache cache = new ResourceInfoCache(new MockBranchClient("test"));

        ResourceInfo root = cache.getResourceInfo("/");
      //  assertEquals("/", root.getName());
        assertEquals("/", root.getPath());
        assertEquals(0, root.getSize());
        assertEquals(ResourceType.DIRECTORY, root.getType());
        assertEquals(2, root.getSubResourceNamesCount());
        if (root.getSubResourceNames(0).equals("a"))
            assertEquals("b", root.getSubResourceNames(1));
        else
            assertEquals("b", root.getSubResourceNames(0));

        ResourceInfo f1 = cache.getResourceInfo("/a/f1.txt");
        assertEquals("/a/f1.txt", f1.getPath());
        assertEquals(0, f1.getSize());
        assertEquals(ResourceType.FILE, f1.getType());
    }
}

