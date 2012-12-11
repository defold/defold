package com.dynamo.cr.tileeditor.scene;

import com.dynamo.tile.proto.Tile.Playback;

public class TextureSetAnimation {

    public static final int COMPONENT_COUNT = 5;
    private String id;
    // u, v, x, y, z
    private final float width;
    private final float height;
    private final float centerX;
    private final float centerY;
    private final int fps;
    private final Playback playback;
    private final int start;
    private final int end;
    private final boolean flipHorizontally;
    private final boolean flipVertically;
    private final boolean isAnimation;

    public TextureSetAnimation(String id, int index,
                               float w, float h, float cx, float cy) {
        this.id = id;
        this.width = w;
        this.height = h;
        this.centerX = cx;
        this.centerY = cy;
        this.fps = 30;
        this.playback = Playback.PLAYBACK_ONCE_FORWARD;
        this.start = index;
        this.end = index;
        this.flipHorizontally = false;
        this.flipVertically = false;
        this.isAnimation = false;
    }

    public TextureSetAnimation(String id,
                               float w, float h, float cx, float cy,
                               Playback playback, int start, int end, int fps,
                               boolean flipHorizontally, boolean flipVertically) {
        this.id = id;
        this.width = w;
        this.height = h;
        this.centerX = cx;
        this.centerY = cy;
        this.playback = playback;
        this.start = start;
        this.end = end;
        this.fps = fps;
        this.flipHorizontally = flipHorizontally;
        this.flipVertically = flipVertically;
        this.isAnimation = true;
    }

    public float getWidth() {
        return width;
    }

    public float getHeight() {
        return height;
    }

    public float getCenterX() {
        return centerX;
    }

    public float getCenterY() {
        return centerY;
    }

    public String getId() {
        return id;
    }

    public int getVertexStart() {
        return start * 6;
    }

    public int getVertexCount() {
        return (end - start + 1) * 6;
    }

    public int getOutlineVertexStart() {
        return start * 4;
    }

    public int getOutlineVertexCount() {
        return (end - start + 1) * 4;
    }

    public int getFps() {
        return fps;
    }

    public Playback getPlayback() {
        return playback;
    }

    public int getStart() {
        return start;
    }

    public int getEnd() {
        return end;
    }

    public boolean isFlipHorizontally() {
        return flipHorizontally;
    }

    public boolean isFlipVertically() {
        return flipVertically;
    }

    public boolean isAnimation() {
        return isAnimation;
    }

}