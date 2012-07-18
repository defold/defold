package com.dynamo.cr.profiler.core;



public class ProfileData {

    public static class Counter {

        public String counter;
        public long value;

        public Counter(String counter, long value) {
            this.counter = counter;
            this.value = value;
        }

    }

    public static class Scope {

        public String scope;
        public long elapsed;
        public long count;

        public Scope(String scope, long elapsed, long count) {
            this.scope = scope;
            this.elapsed = elapsed;
            this.count = count;
        }
    }

    public static class Sample {
        public String name;
        public String scope;
        public long start;
        public long elapsed;
        public int threadId;

        public Sample(String name, String scope, long start, long elapsed,
                int threadId) {
            this.name = name;
            this.scope = scope;
            this.start = start;
            this.elapsed = elapsed;
            this.threadId = threadId;
        }
    }

    public static class Frame {

        private Sample[] samples;
        private Scope[] scopes;
        private Counter[] counters;

        public Frame(Sample[] samples, Scope[] scopes, Counter[] counters) {
            this.samples = samples;
            this.scopes = scopes;
            this.counters = counters;
        }

        public Sample[] getSamples() {
            return samples;
        }

        public Scope[] getScopes() {
            return scopes;
        }

        public Counter[] getCounters() {
            return counters;
        }
    }

    private Frame[] frames;

    public ProfileData(Frame[] frames) {
        this.frames = frames;
    }

    public Frame[] getFrames() {
        return frames;
    }

}
