package com.dynamo.cr.common.cache.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import java.io.File;
import java.io.FileWriter;
import java.io.StringReader;

import org.apache.commons.io.IOUtils;
import org.junit.Test;

import com.dynamo.cr.common.cache.ETagCache;

public class ETagCacheTest {

    @Test
    public void testCache() throws Exception {

        ETagCache etagCache = new ETagCache(2);

        String etag;
        etag = etagCache.getETag(new File("build.properties"));
        assertNotNull(etag);
        assertEquals(0, etagCache.getCacheHits());
        assertEquals(1, etagCache.getCacheMisses());

        etag = etagCache.getETag(new File("does_not_exists"));
        assertNull(etag);
        assertEquals(0, etagCache.getCacheHits());
        // We don't count non-existing files as cache-misses
        assertEquals(1, etagCache.getCacheMisses());

        etag = etagCache.getETag(new File("build.properties"));
        assertNotNull(etag);
        assertEquals(1, etagCache.getCacheHits());
        assertEquals(1, etagCache.getCacheMisses());

        etag = etagCache.getETag(new File("pom.xml"));
        assertNotNull(etag);
        assertEquals(1, etagCache.getCacheHits());
        assertEquals(2, etagCache.getCacheMisses());

        etag = etagCache.getETag(new File("META-INF/MANIFEST.MF"));
        assertNotNull(etag);
        assertEquals(1, etagCache.getCacheHits());
        assertEquals(3, etagCache.getCacheMisses());

        etag = etagCache.getETag(new File("pom.xml"));
        assertNotNull(etag);
        assertEquals(2, etagCache.getCacheHits());
        assertEquals(3, etagCache.getCacheMisses());

        // We should get a cache miss now. The capacity is 2 (MANIFEST.MF and pom.xml in lru-cache)
        etag = etagCache.getETag(new File("build.properties"));
        assertNotNull(etag);
        assertEquals(2, etagCache.getCacheHits());
        assertEquals(4, etagCache.getCacheMisses());
    }

    @Test
    public void testModified() throws Exception {
        ETagCache etagCache = new ETagCache(2);

        File tempFile = File.createTempFile("foo", "bar");
        tempFile.deleteOnExit();

        FileWriter writer = new FileWriter(tempFile);
        IOUtils.copy(new StringReader("foo"), writer);
        writer.close();

        String etag;
        etag = etagCache.getETag(tempFile);
        assertNotNull(etag);
        assertEquals(0, etagCache.getCacheHits());
        assertEquals(1, etagCache.getCacheMisses());

        // lastModified resolution can be in seconds
        Thread.sleep(1000);

        // Rewrite file with same content
        writer = new FileWriter(tempFile);
        IOUtils.copy(new StringReader("foo"), writer);
        writer.close();

        String etag2 = etagCache.getETag(tempFile);
        assertEquals(etag, etag2);
        assertEquals(0, etagCache.getCacheHits());
        assertEquals(2, etagCache.getCacheMisses());

        etag = etagCache.getETag(tempFile);
        assertEquals(etag, etag2);
        assertEquals(1, etagCache.getCacheHits());
        assertEquals(2, etagCache.getCacheMisses());
    }

    @Test
    public void testInvalid() throws Exception {
        ETagCache etagCache = new ETagCache(2);
        String etag;
        etag = etagCache.getETag(new File("."));
        assertNull(etag);
    }

}
