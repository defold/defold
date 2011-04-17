package com.dynamo.cr.editor.fs.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.io.OutputStream;
import java.net.URI;

import javax.ws.rs.core.UriBuilder;

import org.eclipse.core.filesystem.EFS;
import org.eclipse.core.filesystem.IFileInfo;
import org.eclipse.core.filesystem.IFileStore;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.editor.fs.RepositoryFileSystemPlugin;

public class RepositoryFileSystemTest {

    @Before
    public void setUp() throws IOException {
        MockBranchClient branchClient = new MockBranchClient("test");
        RepositoryFileSystemPlugin.setClientFactory(new MockClientFactory(branchClient));
    }

    static URI makeURI(String path) {
        return UriBuilder.fromPath("/dummy_branch").host("localhost").port(1234).scheme("crepo").queryParam("path", path).build();
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
    public void testModify() throws Exception {
        IFileStore store = EFS.getStore(makeURI("/a/f1.txt"));
        IFileInfo preInfo = store.fetchInfo();
        Thread.sleep(1100); // NOTE: Resolution can be in seconds...
        store = EFS.getStore(makeURI("/a//f1.txt"));
        OutputStream os = store.openOutputStream(EFS.NONE, new NullProgressMonitor());
        os.close();
        IFileInfo postInfo = store.fetchInfo();
        assertTrue(postInfo.getLastModified() != preInfo.getLastModified());
    }

}

