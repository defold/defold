package com.dynamo.bob.fs.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

import org.apache.commons.io.FilenameUtils;
import org.eclipse.core.runtime.FileLocator;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Platform;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.osgi.framework.Bundle;

import com.dynamo.bob.ClassLoaderResourceScanner;
import com.dynamo.bob.OsgiResourceScanner;
import com.dynamo.bob.fs.ClassLoaderMountPoint;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.fs.FileSystemWalker;
import com.dynamo.bob.fs.IFileSystem.IWalker;
import com.dynamo.bob.fs.IResource;

public class FileSystemTest {

    DefaultFileSystem fileSystem;

    @Before
    public void setUp() throws Exception {
        this.fileSystem = new DefaultFileSystem();
        Bundle bundle = Platform.getBundle("com.dynamo.cr.bob.test");
        this.fileSystem.setRootDirectory(FileLocator.resolve(FileLocator.find(bundle, new Path("server_root"), null)).getPath());
    }

    @After
    public void tearDown() throws Exception {
        this.fileSystem.close();
    }

    @Test
    public void testWalker() throws Exception {
        IWalker walker = new ZipWalker();
        List<String> results = new ArrayList<String>();
        this.fileSystem.walk(".", walker, results);
        assertFalse(results.isEmpty());
        assertTrue(results.get(0).equals("test_lib.zip"));
    }
    
    private static class ZipWalker extends FileSystemWalker {
        @Override
        public void handleFile(String path, Collection<String> results) {
            if (FilenameUtils.getExtension(path).equals("zip")) {
                results.add(path);
            }
        }
    }
}
