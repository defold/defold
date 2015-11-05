package com.defold.editor;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;

import javax.media.opengl.GL2;

import javafx.application.Platform;
import javafx.concurrent.Task;
import javafx.scene.image.ImageView;
import javafx.scene.image.PixelFormat;
import javafx.scene.image.WritableImage;

// TODO: Dispose
public class AsyncCopier {

    private static final int N_BUFFERS = 2;

    private List<Buffer> buffers = new ArrayList<>();
    private int pboIndex = 0;
    private int width;
    private int height;
    private ExecutorService threadPool;
    private ImageView imageView;

    private static class Buffer {
        int pbo;
        WritableImage image;

        Buffer(int pbo) {
            this.pbo = pbo;
        }

        void setSize(GL2 gl, int width, int height) {
            long data_size = width * height * 4;
            gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, pbo);
            gl.glBufferData(GL2.GL_PIXEL_PACK_BUFFER, data_size, null, GL2.GL_STREAM_READ);
            gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, 0);
            image = new WritableImage(width, height);
        }
    }

    public AsyncCopier(GL2 gl, ExecutorService threadPool, ImageView imageView, int width, int height) {
        this.threadPool = threadPool;
        this.imageView = imageView;
        this.width = width;
        this.height = height;
        IntBuffer pboBuffers = IntBuffer.allocate(N_BUFFERS);
        gl.glGenBuffers(N_BUFFERS, pboBuffers);
        int[] pbos = pboBuffers.array();
        for (int i = 0; i < N_BUFFERS; ++i) {
            Buffer b = new Buffer(pbos[i]);
            buffers.add(b);
        }
        setSize(gl, width, height);
    }

    static class Sample {

        String name;
        long start;
        long end;
        int index;

        public Sample(String name, long start, int index) {
            this.name = name;
            this.start = start;
            this.index = index;
        }

    }

    ArrayList<Sample> samples = new ArrayList<>();

    private Task<Void> task;

    private Sample frame;

    Sample begin(String name, int index) {
        return new Sample(name, System.currentTimeMillis(), index);
    }

    synchronized void end(Sample sample) {
        sample.end = System.currentTimeMillis();
        samples.add(sample);
    }

    public synchronized void dumpSamples(String filename, int removeFirst) throws IOException {
        Collections.sort(samples, new Comparator<Sample>() {

            @Override
            public int compare(Sample o1, Sample o2) {
                return Long.compare(o1.start, o2.start);
            }

        });

        for (int i = 0; i < removeFirst; i++) {
            samples.remove(0);
        }

        try (PrintStream out = new PrintStream(new FileOutputStream(filename))) {
            out.println("start,end,type,index");
            long min = Long.MAX_VALUE;
            for (Sample s : samples) {
                min = Math.min(min, s.start);
            }
            for (Sample s : samples) {
                out.format("%d,%d,%s,%d,%n", s.start - min, s.end - min, s.name, s.index);
            }
        } catch (FileNotFoundException e) {
            throw new IOException(e);
        }
    }

    public void beginFrame(GL2 gl) throws InterruptedException {

        Sample swapSample = begin("swap", -1);

        if (task != null) {
            try {
                Sample wait = begin("wait", -1);
                task.get();
                end(wait);
            } catch (ExecutionException e) {
                e.printStackTrace();
            }
        }

        pboIndex = (pboIndex + 1) % N_BUFFERS;
        int nextPboIndex = (pboIndex + 1) % N_BUFFERS;

        Buffer readTo = buffers.get(pboIndex);
        Buffer swap = buffers.get(nextPboIndex);

        gl.glReadBuffer(GL2.GL_FRONT);
        gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, readTo.pbo);
        int type = GL2.GL_UNSIGNED_INT_8_8_8_8_REV;
        Sample readPixelsSample = begin("readPixels", readTo.pbo);
        gl.glReadPixels(0, 0, width, height, GL2.GL_BGRA, type, 0);
        end(readPixelsSample);

        gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, swap.pbo);
        Sample mapBuffer = begin("mapBuffer", swap.pbo);
        ByteBuffer buffer = gl.glMapBuffer(GL2.GL_PIXEL_PACK_BUFFER, GL2.GL_READ_ONLY);
        /*ByteBuffer buffer = gl.glMapBufferRange(GL2.GL_PIXEL_PACK_BUFFER, 0, width * height * 4, GL2.GL_MAP_READ_BIT | GL2.GL_MAP_INVALIDATE_BUFFER_BIT);
        if (buffer == null) {
            int e = gl.glGetError();
            System.out.println(e);
            return;
        }*/
        end(mapBuffer);

        PixelFormat<IntBuffer> pf = PixelFormat.getIntArgbPreInstance();

        task = new Task<Void>() {

            @Override
            protected Void call() throws Exception {
                Sample setPixels = begin("setPixels", swap.pbo);
                swap.image.getPixelWriter().setPixels(0, 0, width, height, pf, buffer.asIntBuffer(), 0);
                end(setPixels);

                Platform.runLater(new Runnable() {
                    @Override
                    public void run() {
                        Sample setImage = begin("setImage", swap.pbo);
                        imageView.setImage(swap.image);
                        gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, swap.pbo);
                        gl.glUnmapBuffer(GL2.GL_PIXEL_PACK_BUFFER);
                        gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, 0);
                        end(setImage);
                    }
                });

                return null;
            }
        };
        threadPool.submit(task);
        end(swapSample);
    }

    public void beginFrame2(GL2 gl)  {

        Sample begin = begin("begin", -1);
        if (frame != null) {
            end(frame);
        }
        frame = begin("frame", -1);

        if (task != null) {
            try {
                Sample wait = begin("wait", -1);
                task.get();
                end(wait);
            } catch (ExecutionException e) {
                throw new RuntimeException(e);
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }

        gl.getContext().makeCurrent();

        pboIndex = (pboIndex + 1) % N_BUFFERS;

        Buffer readTo = buffers.get(pboIndex);

        gl.glReadBuffer(GL2.GL_FRONT);
        gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, readTo.pbo);
        int type = GL2.GL_UNSIGNED_INT_8_8_8_8_REV;
        Sample readPixels = begin("readPixels", readTo.pbo);
        gl.glReadPixels(0, 0, width, height, GL2.GL_BGRA, type, 0);
        end(readPixels);

        end(begin);


        //gl.glDrawBuffer(GL2.GL_BACK);
    }

    public void endFrame(GL2 gl) {
        gl.getContext().release();

        Sample end = begin("end", -1);

        int nextPboIndex = (pboIndex + 1) % N_BUFFERS;
        Buffer swap = buffers.get(nextPboIndex);
        task = new Task<Void>() {

            @Override
            protected Void call() throws Exception {
                gl.getContext().makeCurrent();
                gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, swap.pbo);
                Sample mapBuffer = begin("mapBuffer", swap.pbo);
                ByteBuffer buffer = gl.glMapBuffer(GL2.GL_PIXEL_PACK_BUFFER, GL2.GL_READ_ONLY);
                end(mapBuffer);

                PixelFormat<IntBuffer> pf = PixelFormat.getIntArgbPreInstance();

                Sample setPixels = begin("setPixels", swap.pbo);
                swap.image.getPixelWriter().setPixels(0, 0, width, height, pf, buffer.asIntBuffer(), width);
                end(setPixels);

                gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, swap.pbo);
                gl.glUnmapBuffer(GL2.GL_PIXEL_PACK_BUFFER);
                gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, 0);
                gl.getContext().release();

                Platform.runLater(new Runnable() {
                    @Override
                    public void run() {
                        Sample setImage = begin("setImage", swap.pbo);
                        imageView.setImage(swap.image);
                        end(setImage);
                    }
                });

                return null;
            }
        };
        threadPool.submit(task);
        end(end);
    }

    public void setSize(GL2 gl, int width, int height) {
        this.width = width;
        this.height = height;
        for (Buffer buffer : buffers) {
            buffer.setSize(gl, width, height);
        }
    }

    public void dispose(GL2 gl) {
        int[] b = new int[buffers.size()];
        for (int i = 0; i < b.length; i++) {
            b[i] = buffers.get(i).pbo;
        }
        gl.glDeleteBuffers(b.length, b, 0);
    }

}
