package com.defold.editor;

import java.nio.ByteBuffer;
import java.nio.IntBuffer;

import com.jogamp.opengl.GL2;
import com.defold.util.Profiler;
import com.defold.util.Profiler.Sample;

import javafx.scene.image.PixelFormat;
import javafx.scene.image.WritableImage;

public class AsyncCopier {

    // NOTE setting N_IMAGES to 1 puts the application in 30 fps
    // [Ragnar] I believe this is due to writing to the same WritableImage
    // currently bound to the ImageView and that it would cause stalls
    // on the texture level, or somewhere else. The 30 fps behaviour has only
    // been seen when writing to the WritableImage while its bound to the ImageView.
    private static final int N_IMAGES = 2;

    // Lazily created pbo, one is enough for copying frame-buffer=>pbo, pbo=>image
    private Buffer buffer;
    private CycleBuffer<ImageSlot> imageSlots = new CycleBuffer<ImageSlot>(N_IMAGES);

    private int width;
    private int height;

    private static class ImageSlot {
        ImageSlot(WritableImage image, int frameVersion) {
            this.image = image;
            this.frameVersion = frameVersion;
        }

        WritableImage image;
        int frameVersion;
    }

    private static class Buffer {
        int pbo;
        int width;
        int height;
        int frameVersion;
        int profilerFrame;
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

        void toImage(GL2 gl, ImageSlot slot) {
            ByteBuffer buffer = gl.glMapBuffer(GL2.GL_PIXEL_PACK_BUFFER, GL2.GL_READ_ONLY);

            if (slot.image.getWidth() != width || slot.image.getHeight() != height) {
                slot.image = new WritableImage(width, height);
            }
            slot.image.getPixelWriter().setPixels(0, 0, width, height, pf, buffer, width * 4);
            slot.frameVersion = this.frameVersion;

            gl.glUnmapBuffer(GL2.GL_PIXEL_PACK_BUFFER);
        }

        void readFrameBufferAsync(GL2 gl, int frameVersion, int profilerFrame) {
            this.frameVersion = frameVersion;
            this.profilerFrame = profilerFrame;
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

    private static class CycleBuffer<T> {
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

        void set(int index, T entry) {
            this.entries[index] = entry;
        }

        @SuppressWarnings("unchecked")
        T next() {
            this.index = (this.index + 1) % this.size;
            return (T)this.entries[this.index];
        }
    }

    public AsyncCopier(int width, int height) {
        this.width = width;
        this.height = height;
        for (int i = 0; i < N_IMAGES; ++i) {
            imageSlots.set(i, new ImageSlot(new WritableImage(width, height), 0));
        }
    }

    public WritableImage flip(GL2 gl, int frameVersion) {
        if (buffer == null) {
            IntBuffer pboBuffers = IntBuffer.allocate(1);
            gl.glGenBuffers(1, pboBuffers);
            int[] pbos = pboBuffers.array();
            this.buffer = new Buffer(pbos[0]);
        }
        ImageSlot slot = this.imageSlots.current();
        if (slot.frameVersion == frameVersion) {
            return slot.image;
        }
        WritableImage result = null;
        buffer.bind(gl);
        if (buffer.frameVersion > 0) {
            Sample toImage = Profiler.begin("render-async", "to-image", buffer.profilerFrame);
            slot = this.imageSlots.next();
            buffer.toImage(gl, slot);
            result = slot.image;
            Profiler.end(toImage);
        }
        if (buffer.frameVersion != frameVersion) {
            Sample readPixels = Profiler.begin("render-async", "read-pixels");
            buffer.setSize(gl, this.width, this.height);
            buffer.readFrameBufferAsync(gl, frameVersion, readPixels.getFrame());
            Profiler.end(readPixels);
        }

        buffer.unbind(gl);
        return result;
    }

    public void setSize(int width, int height) {
        this.width = width;
        this.height = height;
    }

    public void dispose(GL2 gl) {
        if (buffer != null) {
            int[] b = {buffer.pbo};
            gl.glDeleteBuffers(1, b, 0);
        }
    }

}
