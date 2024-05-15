// Copyright 2020-2024 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.defold.util;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedList;
import java.util.concurrent.atomic.AtomicInteger;

import org.codehaus.jackson.node.ArrayNode;
import org.codehaus.jackson.node.JsonNodeFactory;
import org.codehaus.jackson.node.ObjectNode;

public class Profiler {

    static int keep = 1000;
    static LinkedList<Sample> samples = new LinkedList<>();
    static Sample frameSample = null;
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

        private Sample(String name, double start, double end, Object user1, int frame) {
            this(name, start, end, user1, "", frame);
        }

        private Sample(String name, double start, double end, Object user1, Object user2, int frame) {
            this.name = name;
            this.start = start;
            this.end = end;
            this.user1 = user1;
            this.user2 = user2;
            this.thread = threadName.get();
            this.frame = frame;
        }

        public int getFrame() {
            return this.frame;
        }

        @Override
        public String toString() {
            return String.format("%s %.2fms [%.2f, %.2f] s", name, end-start, start, end, user1);
        }
    }

    public static int beginFrame() {
        int frame = frameNumber.incrementAndGet();
        if (frameSample != null) {
            end(frameSample);
        }
        frameSample = begin("frame", -1, frame);
        return frame;
    }

    public static Sample begin(String name, Object user) {
        return begin(name, user, frameNumber.get());
    }

    public static Sample begin(String name, Object user, int frame) {
        double t = System.nanoTime() / 1000000.0;
        return new Sample(name, t, t, user, frame);
    }

    public synchronized static void end(Sample s) {
        s.end = System.nanoTime() / 1000000.0;
        samples.add(s);
        while (samples.size() > keep) {
            samples.remove();
        }
    }

    public static synchronized void reset()  {
        samples.clear();
    }

    public static synchronized String dumpJson() throws IOException {
        Collections.sort(samples, new Comparator<Sample>() {
            @Override
            public int compare(Sample o1, Sample o2) {
                return Double.compare(o1.start, o2.start);
            }
        });
        JsonNodeFactory f = JsonNodeFactory.instance;
        ArrayNode smpls = new ArrayNode(f);
        double min = Double.MAX_VALUE;
        for (Sample s : samples) {
            min = Math.min(min, s.start);
        }
        for (Sample s : samples) {
            ObjectNode o = smpls.addObject();
            o.put("start", s.start - min);
            o.put("end", s.end - min);
            o.put("name", s.name);
            o.put("user1", s.user1.toString());
            o.put("user2", s.user2.toString());
            o.put("thread", s.thread);
            o.put("frame", s.frame);
        }
        return smpls.toString();
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
