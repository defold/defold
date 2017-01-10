package com.defold.editor;

import java.nio.ByteBuffer;
import java.nio.IntBuffer;

import com.jogamp.opengl.GL2;
import com.defold.util.Profiler;
import com.defold.util.Profiler.Sample;

import javafx.scene.image.PixelFormat;
import javafx.scene.image.WritableImage;

public class AsyncCopier {

    // Amount of PBOs to copy between, one should be enough
    private static final int N_BUFFERS = 1;
    // NOTE setting N_IMAGES to 1 puts the application in 30 fps
    // [Ragnar] I believe this is due to writing to the same WritableImage
    // currently bound to the ImageView and that it would cause stalls
    // on the texture level, or somewhere else. The 30 fps behaviour has only
    // been seen when writing to the WritableImage while its bound to the ImageView.
    private static final int N_IMAGES = 2;

    // Lazily created
    private CycleBuffer<Buffer> buffers;
    private CycleBuffer<WritableImage> images = new CycleBuffer<WritableImage>(N_IMAGES);

    private int width;
    private int height;
    private int currentFrame;

    private static class Buffer {
        int pbo;
        int width;
        int height;
        int frame;
        PixelFormat<ByteBuffer> pf;

        Buffer(int pbo) {
            this.pbo = pbo;
            // NOTE You have to know what you are doing if you want to change this value.
            // If it does not match the native format exactly, image.getPixelWriter().setPixels(...) below will take a lot more time.
            pf = PixelFormat.getByteBgraPreInstance();
        }

        void bind(GL2 gl) {
            gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, pbo);
        }

        void unbind(GL2 gl) {
            gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, 0);
        }

        WritableImage toImage(GL2 gl, WritableImage image) {
            ByteBuffer buffer = gl.glMapBuffer(GL2.GL_PIXEL_PACK_BUFFER, GL2.GL_READ_ONLY);

            if (image.getWidth() != width || image.getHeight() != height) {
                image = new WritableImage(width, height);
            }
            image.getPixelWriter().setPixels(0, 0, width, height, pf, buffer, width * 4);

            gl.glUnmapBuffer(GL2.GL_PIXEL_PACK_BUFFER);
            return image;
        }

        void readFrameBufferAsync(GL2 gl, int frame) {
            this.frame = frame;
            // NOTE You have to know what you are doing if you want to change this value.
            // If it does not match the native format exactly, glReadPixles will take a lot more time.
            int type = GL2.GL_UNSIGNED_INT_8_8_8_8_REV;
            gl.glReadPixels(0, 0, this.width, this.height, GL2.GL_BGRA, type, 0);
        }

        void setSize(GL2 gl, int width, int height) {
            if (this.width == width && this.height == height) {
                return;
            }
            this.width = width;
            this.height = height;
            long data_size = width * height * 4;
            gl.glBufferData(GL2.GL_PIXEL_PACK_BUFFER, data_size, null, GL2.GL_STREAM_READ);
        }
    }

    private class CycleBuffer<T> {
        int size;
        int index;
        Object[] entries;

        CycleBuffer(int size) {
            this.index = 0;
            this.size = size;
            this.entries = new Object[size];
        }

        @SuppressWarnings("unchecked")
        T current() {
            return (T)this.entries[this.index];
        }

        @SuppressWarnings("unchecked")
        T get(int index) {
            return (T)this.entries[index];
        }
        void set(int index, T entry) {
            this.entries[index] = entry;
        }

        void setCurrent(T entry) {
            this.entries[this.index] = entry;
        }

        void next() {
            this.index = (this.index + 1) % this.size;
        }

        int size() {
            return this.size;
        }
    }

    public AsyncCopier(int width, int height) {
        this.width = width;
        this.height = height;
        for (int i = 0; i < N_IMAGES; ++i) {
            images.set(i, new WritableImage(width, height));
        }
        this.currentFrame = 0;
    }

    private void createBuffers(GL2 gl) {
        this.buffers = new CycleBuffer<Buffer>(N_BUFFERS);
        IntBuffer pboBuffers = IntBuffer.allocate(N_BUFFERS);
        gl.glGenBuffers(N_BUFFERS, pboBuffers);
        int[] pbos = pboBuffers.array();
        for (int i = 0; i < N_BUFFERS; ++i) {
            this.buffers.set(i, new Buffer(pbos[i]));
        }
    }

    public WritableImage flip(GL2 gl) {
        final Sample end = Profiler.begin("render-async", "end-frame");

        if (buffers == null) {
            createBuffers(gl);
        }
        Buffer currentBuffer = buffers.current();
        buffers.next();
        currentBuffer.bind(gl);
        WritableImage image = null;
        if (currentFrame >= N_BUFFERS) {
            Sample toImage = Profiler.begin("render-async", "to-image", currentBuffer.frame);
            image = this.images.current();
            image = currentBuffer.toImage(gl, image);
            this.images.setCurrent(image);
            this.images.next();
            Profiler.end(toImage);
        }
        Sample readPixels = Profiler.begin("render-async", "read-pixels");
        currentBuffer.setSize(gl, this.width, this.height);
        currentBuffer.readFrameBufferAsync(gl, readPixels.getFrame());
        Profiler.end(readPixels);

        currentBuffer.unbind(gl);
        ++this.currentFrame;
        Profiler.end(end);
        return image;
    }

    public void setSize(int width, int height) {
        this.width = width;
        this.height = height;
    }

    public void dispose(GL2 gl) {
        int[] b = new int[buffers.size()];
        for (int i = 0; i < b.length; i++) {
            b[i] = buffers.get(i).pbo;
        }
        gl.glDeleteBuffers(b.length, b, 0);
    }

}
