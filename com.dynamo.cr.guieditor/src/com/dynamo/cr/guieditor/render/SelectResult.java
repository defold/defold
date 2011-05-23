package com.dynamo.cr.guieditor.render;

import java.util.List;

public class SelectResult
{
    public static class Pair implements Comparable<Pair> {
        Pair(long z, int index) {
            this.z = z;
            this.index = index;
        }
        public long z;
        public int index;

        @Override
        public int compareTo(Pair o) {
            return (z<o.z ? -1 : (z==o.z ? 0 : 1));
        }

        @Override
        public String toString() {
            return String.format("%d (%d)", index, z);
        }
    }
    public SelectResult(List<Pair> selected, long minz)
    {
        this.selected = selected;
        minZ = minz;

    }
    public List<Pair> selected;
    public long minZ = Long.MAX_VALUE;
}

