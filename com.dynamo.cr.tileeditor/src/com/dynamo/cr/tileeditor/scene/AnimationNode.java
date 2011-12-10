package com.dynamo.cr.tileeditor.scene;

import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.validators.Unique;
import com.dynamo.tile.proto.Tile;

public class AnimationNode extends Node {

    @Property
    @NotEmpty
    @Unique
    private String id;

    @Property
    private int startTile;

    @Property
    private int endTile;

    @Property
    private Tile.Playback2 playback;

    @Property
    private int fps;

    @Property
    private boolean flipHorizontal;

    @Property
    private boolean flipVertical;

    public String getId() {
        return this.id;
    }

    public void setId(String id) {
        this.id = id;
    }

    public int getStartTile() {
        return this.startTile;
    }

    public void setStartTile(int startTile) {
        this.startTile = startTile;
    }

    public TileSetNode getTileSetNode() {
        return (TileSetNode) getParent().getParent();
    }

    public int getEndTile() {
        return this.endTile;
    }

    public void setEndTile(int endTile) {
        this.endTile = endTile;
    }

    public Tile.Playback2 getPlayback() {
        return this.playback;
    }

    public void setPlayback(Tile.Playback2 playback) {
        this.playback = playback;
    }

    public int getFps() {
        return this.fps;
    }

    public void setFps(int fps) {
        this.fps = fps;
    }

    public boolean isFlipHorizontal() {
        return this.flipHorizontal;
    }

    public void setFlipHorizontal(boolean flipHorizontal) {
        this.flipHorizontal = flipHorizontal;
    }

    public boolean isFlipVertical() {
        return this.flipVertical;
    }

    public void setFlipVertical(boolean flipVertical) {
        this.flipVertical = flipVertical;
    }

}
