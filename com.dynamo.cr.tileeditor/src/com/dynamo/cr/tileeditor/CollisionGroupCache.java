package com.dynamo.cr.tileeditor;

import java.util.HashMap;
import java.util.Map;
import java.util.Stack;

public class CollisionGroupCache {
    private static class Entry {
        public final int index;
        public int count;

        public Entry(int index) {
            this.index = index;
            this.count = 0;
        }
    }

    public static int SIZE = 16;

    private final Stack<Integer> indexPool;
    private final Map<String, Entry> groups;

    public CollisionGroupCache() {
        this.indexPool = new Stack<Integer>();
        for (int i = 0; i < SIZE; ++i) {
            this.indexPool.push(SIZE - i - 1);
        }
        this.groups = new HashMap<String, Entry>();
    }

    public void add(String name) {
        Entry entry = this.groups.get(name);
        if (entry == null && !isFull()) {
            entry = new Entry(this.indexPool.pop());
            this.groups.put(name, entry);
        }
        if (entry != null) {
            ++entry.count;
        }
    }

    public void remove(String name) {
        Entry entry = this.groups.get(name);
        if (entry != null) {
            --entry.count;
            if (entry.count == 0) {
                this.indexPool.push(entry.index);
                this.groups.remove(name);
            }
        }
    }

    public int get(String name) {
        Entry entry = this.groups.get(name);
        if (entry != null) {
            return entry.index;
        }
        return -1;
    }

    private boolean isFull() {
        return this.indexPool.isEmpty();
    }
}
