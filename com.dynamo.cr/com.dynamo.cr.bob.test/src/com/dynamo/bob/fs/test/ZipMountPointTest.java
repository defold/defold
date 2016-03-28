package com.dynamo.bob.fs.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.util.ArrayList;
import java.util.Collection;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import com.dynamo.bob.fs.FileSystemWalker;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ZipMountPoint;
import com.dynamo.bob.test.TestLibrariesRule;

public class ZipMountPointTest {

    ZipMountPoint mp;

    @Rule
    public TestLibrariesRule testLibs = new TestLibrariesRule();

    @Before
    public void setUp() throws Exception {
        this.mp = new ZipMountPoint(null, "server_root/test_lib1.zip");
        this.mp.mount();
    }

    @After
    public void tearDown() throws Exception {
        this.mp.unmount();
    }

    @Test
    public void testResource() throws Exception {
        IResource resource = mp.get("test_lib1/file1.in");
        assertTrue(resource != null);
        assertEquals(new String(resource.getContent()), "file1");
    }

    @Test
    public void testMount() throws Exception {
        ZipMountPoint mp = new ZipMountPoint(null, "server_root/test_lib2.zip");
        assertTrue(mp.get("test_lib2/file2.in") == null);
        mp.mount();
        assertTrue(mp.get("test_lib2/file2.in") != null);
        mp.unmount();
        assertTrue(mp.get("test_lib2/file2.in") == null);
    }

    @Test
    public void testWalker() throws Exception {
        FileSystemWalker walker = new FileSystemWalker();
        Collection<String> results = new ArrayList<String>();
        this.mp.walk(".", walker, results);
        assertEquals(1, results.size());
        assertTrue(results.contains("test_lib1/file1.in"));
    }

    @Test
    public void testWalkerWithSubdir() throws Exception {
    	
    	// Mount a library zip that "incorrectly" has a subdir before its game.project
    	ZipMountPoint mp = new ZipMountPoint(null, "server_root/test_lib3.zip");
    	mp.mount();
        FileSystemWalker walker = new FileSystemWalker();
        Collection<String> results = new ArrayList<String>();
        mp.walk(".", walker, results);
        mp.unmount();
        
        // Should result in paths without subdir
        assertEquals(1, results.size());
        assertTrue(results.contains("test_lib3/file3.in"));
        
    }

    @Test
    public void testWalkerWithSubdir2() throws Exception {
    	ZipMountPoint mp = new ZipMountPoint(null, "server_root/test_lib4.zip");
    	mp.mount();
        FileSystemWalker walker = new FileSystemWalker();
        Collection<String> results = new ArrayList<String>();
        mp.walk(".", walker, results);
        mp.unmount();
        assertEquals(1, results.size());
        assertTrue(results.contains("test_lib4/file4.in"));
    }
}

