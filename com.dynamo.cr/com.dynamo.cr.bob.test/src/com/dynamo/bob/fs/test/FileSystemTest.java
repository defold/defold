package com.dynamo.bob.fs.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

import org.apache.commons.io.FilenameUtils;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.fs.FileSystemWalker;
import com.dynamo.bob.fs.IFileSystem.IWalker;
import com.dynamo.bob.test.TestLibrariesRule;

public class FileSystemTest {

    DefaultFileSystem fileSystem;

    @Rule
    public TestLibrariesRule testLibs = new TestLibrariesRule();

    @Before
    public void setUp() throws Exception {
        this.fileSystem = new DefaultFileSystem();
        this.fileSystem.setRootDirectory(testLibs.getServerLocation());
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
        assertTrue(results.contains("test_lib1.zip"));
        assertTrue(results.contains("test_lib2.zip"));
        assertTrue(results.contains("test_lib3.zip"));
        assertTrue(results.contains("test_lib4.zip"));
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
