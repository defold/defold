package com.defold.editor;

import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.TimeUnit;

import javax.media.opengl.GL2;

import com.defold.editor.Profiler.Sample;

import javafx.application.Platform;
import javafx.concurrent.Task;
import javafx.scene.image.Image;
import javafx.scene.image.ImageView;
import javafx.scene.image.PixelFormat;
import javafx.scene.image.WritableImage;

public class AsyncCopier {

    private static final int N_BUFFERS = 2;
    private List<Buffer> buffers = new ArrayList<>();
    private int pboIndex = 0;
    private int width;
    private int height;
    private ExecutorService threadPool;
    private ImageView imageView;

    private BlockingQueue<WritableImage> freeImages = new ArrayBlockingQueue<>(N_BUFFERS);
    private Task<Void> task;
    private Sample frame;

    private static class Buffer {
        int pbo;
        //WritableImage image;

        Buffer(int pbo) {
            this.pbo = pbo;
        }

        void setSize(GL2 gl, int width, int height) {
            long data_size = width * height * 4;
            gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, pbo);
            gl.glBufferData(GL2.GL_PIXEL_PACK_BUFFER, data_size, null, GL2.GL_STREAM_READ);
            gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, 0);
          //  image = new WritableImage(width, height);
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
            freeImages.add(new WritableImage(width, height));
        }
        setSize(gl, width, height);
    }

    public void beginFrame(GL2 gl)  {
        Profiler.beginFrame();
        Sample begin = Profiler.begin("begin", -1);
        if (frame != null) {
            Profiler.add(frame);
        }
        frame = Profiler.begin("frame", -1);

        if (task != null) {
            try {
                Sample wait = Profiler.begin("wait", -1);
                task.get();
                task = null;
                Profiler.add(wait);
            } catch (ExecutionException e) {
                throw new RuntimeException(e);
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }

        gl.getContext().makeCurrent();

        Profiler.add(begin);
    }

    public void endFrame(GL2 gl) {
        Sample end = Profiler.begin("end", -1);

        pboIndex = (pboIndex + 1) % N_BUFFERS;
        Buffer readTo = buffers.get(pboIndex);
        gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, readTo.pbo);
        int type = GL2.GL_UNSIGNED_INT_8_8_8_8_REV;
        Sample readPixels = Profiler.begin("readPixels", readTo.pbo);
        gl.glReadPixels(0, 0, width, height, GL2.GL_BGRA, type, 0);
        Profiler.add(readPixels);

        gl.getContext().release();

        if (task != null) {
            throw new RuntimeException("task in flight!");
        }

        task = new Task<Void>() {

            @Override
            protected Void call() throws Exception {
                gl.getContext().makeCurrent();

                try {
                    gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, readTo.pbo);
                    Sample mapBuffer = Profiler.begin("mapBuffer", readTo.pbo);
                    ByteBuffer buffer = gl.glMapBuffer(GL2.GL_PIXEL_PACK_BUFFER, GL2.GL_READ_ONLY);
                    Profiler.add(mapBuffer);

                    PixelFormat<IntBuffer> pf = PixelFormat.getIntArgbPreInstance();

                    /*
                    if (readTo.image == imageView.getImage()) {
                        System.out.println("Image in use...");
                        gl.getContext().release();
                        return null;
                    }

                    if (buffer == null) {
                        System.out.println("Buffer is null...");
                        gl.getContext().release();
                        return null;
                    }
    */

                    Sample setPixels = Profiler.begin("setPixels", readTo.pbo);
                    //readTo.image = new WritableImage(width, height);

                    WritableImage tmpImage = freeImages.poll(1, TimeUnit.MILLISECONDS);
                    if (tmpImage != null) {
                        if (tmpImage.getWidth() != width || tmpImage.getHeight() != height) {
                            tmpImage = new WritableImage(width, height);
                        }
                        tmpImage.getPixelWriter().setPixels(0, 0, width, height, pf, buffer.asIntBuffer(), width);
                    } else {
                        System.out.println("no image available");
                    }

                    WritableImage image = tmpImage;

                    /*
                    synchronized (AsyncCopier.this) {
                        if (readTo.image != imageView.getImage()) {
                            readTo.image.getPixelWriter().setPixels(0, 0, width, height, pf, buffer.asIntBuffer(), width);
                        } else {
                            System.out.println("image in use");
                        }
                    }*/
                    Profiler.add(setPixels);

                    gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, readTo.pbo);
                    gl.glUnmapBuffer(GL2.GL_PIXEL_PACK_BUFFER);
                    gl.glBindBuffer(GL2.GL_PIXEL_PACK_BUFFER, 0);

                    if (image != null) {
                        Platform.runLater(new Runnable() {
                            @Override
                            public void run() {
                                Sample setImage = Profiler.begin("setImage", readTo.pbo);
                                //readTo.image.set
                                Image oldImage = imageView.getImage();
                                if (oldImage != null) {
                                    try {
                                        freeImages.put((WritableImage) oldImage);
                                    } catch (InterruptedException e) {
                                        e.printStackTrace();
                                    }
                                }
                                imageView.setImage(image);

                                /*synchronized (AsyncCopier.this) {
                                    imageView.setImage(readTo.image);
                                }*/
                                Profiler.add(setImage);
                            }
                        });
                    }

                } finally {
                    gl.getContext().release();
                }

                return null;
            }
        };
        threadPool.submit(task);
        Profiler.add(end);
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
