package com.defold.editor;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedList;
import java.util.concurrent.atomic.AtomicInteger;

public class Profiler {

    static int keep = 1000;
    static LinkedList<Sample> samples = new LinkedList<>();
    static AtomicInteger frameNumber = new AtomicInteger(0);

    private static final ThreadLocal<String> threadName =
            new ThreadLocal<String>() {
                @Override
                protected String initialValue() {
                    return Thread.currentThread().getName();
                }
    };

    public static class Sample {
        String name;
        double start;
        double end;
        Object user1;
        Object user2;
        String thread;
        int frame = 0;

        private Sample(String name, double start, double end, Object user1) {
            this(name, start, end, user1, "");
        }

        private Sample(String name, double start, double end, Object user1, Object user2) {
            this.name = name;
            this.start = start;
            this.end = end;
            this.user1 = user1;
            this.user2 = user2;
            this.thread = threadName.get();
            this.frame = frameNumber.get();
        }

        @Override
        public String toString() {
            return String.format("%s %.2fms [%.2f, %.2f] s", name, end-start, start, end, user1);
        }
    }

    public static void beginFrame() {
        frameNumber.incrementAndGet();
    }

    public static Sample begin(String name, Object user) {
        double t = System.nanoTime() / 1000000.0;
        return new Sample(name, t, t, user);
    }

    public synchronized static void add(Sample s) {
        s.end = System.nanoTime() / 1000000.0;
        samples.add(s);
        while (samples.size() > keep) {
            samples.remove();
        }
    }

    public static synchronized void reset()  {
        samples.clear();
    }

    public static synchronized void dump(String filename) throws IOException {
        Collections.sort(samples, new Comparator<Sample>() {

            @Override
            public int compare(Sample o1, Sample o2) {
                return Double.compare(o1.start, o2.start);
            }

        });

        try (PrintStream out = new PrintStream(new FileOutputStream(filename))) {
            out.println("start,end,name,user1,user2,thread,frame");
            double min = Double.MAX_VALUE;
            for (Sample s : samples) {
                min = Math.min(min, s.start);
            }
            for (Sample s : samples) {
                out.format("%f,%f,%s,%s,%s,%s,%s,%n", s.start - min, s.end - min, s.name, s.user1, s.user2, s.thread, s.frame);
            }
            System.out.format("Profile %s written%n", filename);
        } catch (FileNotFoundException e) {
            throw new IOException(e);
        }

    }
}
