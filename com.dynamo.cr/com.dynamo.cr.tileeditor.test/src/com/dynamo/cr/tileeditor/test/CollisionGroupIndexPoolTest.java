package com.dynamo.cr.tileeditor.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.tileeditor.Activator;
import com.dynamo.cr.tileeditor.scene.CollisionGroupIndexPool;

public class CollisionGroupIndexPoolTest {

    private CollisionGroupIndexPool cache;
    private int count;

    @Before
    public void setup() {
        this.count = Activator.MAX_COLLISION_GROUP_COUNT;
        this.cache = new CollisionGroupIndexPool(this.count);
    }

    @Test
    public void testRefCount() {
        String id = "test";
        assertThat(this.cache.get(id), is(-1));
        assertTrue(this.cache.add(id));
        assertTrue(this.cache.add(id));
        assertThat(this.cache.get(id), is(0));
        this.cache.remove(id);
        this.cache.remove(id);
        assertThat(this.cache.get(id), is(-1));
    }

    @Test
    public void testFull() {
        for (int i = 0; i < this.count; ++i) {
            String id = "" + i;
            assertTrue(this.cache.add(id));
            assertThat(this.cache.get(id), is(i));
        }
        String test = "test";
        assertFalse(this.cache.add(test));
        assertThat(this.cache.get(test), is(-1));
        this.cache.remove("0");
        assertTrue(this.cache.add(test));
        assertThat(this.cache.get(test), is(0));
    }

    @Test
    public void removeMiddle() {
        for (int i = 0; i < this.count; ++i) {
            String id = "" + i;
            assertTrue(this.cache.add(id));
            assertThat(this.cache.get(id), is(i));
        }
        this.cache.remove("4");
        assertTrue(this.cache.add("test"));
        assertThat(this.cache.get("test"), is(4));
    }
}
