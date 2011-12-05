package com.dynamo.cr.tileeditor.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.tileeditor.CollisionGroupCache;

public class CollisionGroupTest {

    private CollisionGroupCache cache;

    @Before
    public void setup() {
        this.cache = new CollisionGroupCache();
    }

    @Test
    public void testRefCount() {
        String id = "test";
        assertThat(this.cache.get(id), is(-1));
        this.cache.add(id);
        this.cache.add(id);
        assertThat(this.cache.get(id), is(0));
        this.cache.remove(id);
        this.cache.remove(id);
        assertThat(this.cache.get(id), is(-1));
    }

    @Test
    public void testFull() {
        for (int i = 0; i < CollisionGroupCache.SIZE; ++i) {
            String id = "" + i;
            this.cache.add(id);
            assertThat(this.cache.get(id), is(i));
        }
        String test = "test";
        this.cache.add(test);
        assertThat(this.cache.get(test), is(-1));
        this.cache.remove("0");
        this.cache.add(test);
        assertThat(this.cache.get(test), is(0));
    }

    @Test
    public void removeMiddle() {
        for (int i = 0; i < CollisionGroupCache.SIZE; ++i) {
            String id = "" + i;
            this.cache.add(id);
            assertThat(this.cache.get(id), is(i));
        }
        this.cache.remove("4");
        this.cache.add("test");
        assertThat(this.cache.get("test"), is(4));
    }
}
