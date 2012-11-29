package com.dynamo.cr.tileeditor.scene;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.tile.proto.Tile.Playback;

@SuppressWarnings("serial")
public class AtlasAnimationNode extends Node {

    private Playback playback;
    private int fps;
    private boolean flipHorizontally;
    private boolean flipVertically;

    public void setPlayback(Playback playback) {
        this.playback = playback;
    }

    public Playback getPlayback() {
        return playback;
    }

    public void setFps(int fps) {
        this.fps = fps;
    }

    public int getFps() {
        return fps;
    }

    public void setFlipHorizontally(boolean flipHorizontally) {
        this.flipHorizontally = flipHorizontally;
    }

    public boolean isFlipHorizontally() {
        return flipHorizontally;
    }

    public void setFlipVertically(boolean flipVertically) {
        this.flipVertically = flipVertically;
    }

    public boolean isFlipVertically() {
        return flipVertically;
    }

}
