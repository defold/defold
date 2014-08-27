package com.dynamo.bob.fs.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.util.ArrayList;
import java.util.Collection;

import org.eclipse.core.runtime.Platform;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.ClassLoaderResourceScanner;
import com.dynamo.bob.OsgiResourceScanner;
import com.dynamo.bob.fs.ClassLoaderMountPoint;
import com.dynamo.bob.fs.FileSystemWalker;
import com.dynamo.bob.fs.IResource;

public class ClassLoaderMountPointTest {

    ClassLoaderMountPoint mp;

    @Before
    public void setUp() throws Exception {
        this.mp = new ClassLoaderMountPoint(null, "com/dynamo/bob/fs/test/included*", new OsgiResourceScanner(Platform.getBundle("com.dynamo.cr.bob")));
        this.mp.mount();
    }

    @After
    public void tearDown() throws Exception {
        this.mp.unmount();
    }

    @Test
    public void testResource() throws Exception {
        IResource resource = this.mp.get("com/dynamo/bob/fs/test/included_resource.txt");
        assertTrue(resource != null);
        assertTrue(new String(resource.getContent()).equals("include"));
    }

    @Test
    public void testExclusion() throws Exception {
        assertFalse(mp.get("com/dynamo/bob/fs/test/excluded_resource.txt") != null);
    }

    @Test
    public void testWalker() throws Exception {
        FileSystemWalker walker = new FileSystemWalker();
        Collection<String> results = new ArrayList<String>();
        this.mp.walk(".", walker, results);
        assertTrue(results.contains("com/dynamo/bob/fs/test/included_resource.txt"));
    }
}
