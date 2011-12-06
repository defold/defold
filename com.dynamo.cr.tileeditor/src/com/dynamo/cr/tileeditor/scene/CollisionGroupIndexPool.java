package com.dynamo.cr.tileeditor.scene;

import java.util.HashMap;
import java.util.Map;
import java.util.Stack;

public class CollisionGroupIndexPool {
    private static class Entry {
        public final int index;
        public int count;

        public Entry(int index) {
            this.index = index;
            this.count = 0;
        }
    }

    private final int size;
    private final Stack<Integer> indexPool;
    private final Map<String, Entry> groups;

    public CollisionGroupIndexPool(int size) {
        this.size = size;
        this.indexPool = new Stack<Integer>();
        this.groups = new HashMap<String, Entry>();
        clear();
    }

    public boolean add(String name) {
        Entry entry = this.groups.get(name);
        if (entry == null && !isFull()) {
            entry = new Entry(this.indexPool.pop());
            this.groups.put(name, entry);
        }
        if (entry != null) {
            ++entry.count;
            return true;
        }
        return false;
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

    public void clear() {
        this.indexPool.clear();
        for (int i = 0; i < this.size; ++i) {
            this.indexPool.push(this.size - i - 1);
        }
        this.groups.clear();
    }

    private boolean isFull() {
        return this.indexPool.isEmpty();
    }
}
