package com.dynamo.cr.tileeditor.scene;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.properties.GreaterThanZero;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Identifiable;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.validators.Unique;
import com.dynamo.cr.tileeditor.Activator;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.Playback;

@SuppressWarnings("serial")
public class AnimationNode extends Node implements Identifiable {

    @Property
    @NotEmpty
    @Unique(scope = AnimationNode.class)
    private String id = "";

    @Property
    @GreaterThanZero
    private int startTile = 1;

    @Property
    @GreaterThanZero
    private int endTile = 1;

    @Property
    private Tile.Playback playback = Playback.PLAYBACK_ONCE_FORWARD;

    @Property
    @GreaterThanZero
    private int fps = 30;

    @Property
    private boolean flipHorizontally;

    @Property
    private boolean flipVertically;

    private int currentTile = 1;
    private float cursor = 0.0f;
    private boolean playing = false;

    public AnimationNode() {
        this.id = "anim";
    }

    @Override
    public void dispose() {
        super.dispose();
        this.playing = false;
    }

    public String getId() {
        return this.id;
    }

    public void setId(String id) {
        this.id = id;
        Node parent = getParent();
        if (parent != null) {
            ((TileSetNode)parent).sortChildren();
        }
    }

    public int getStartTile() {
        return this.startTile;
    }

    public void setStartTile(int startTile) {
        this.startTile = startTile;
        this.currentTile = startTile;
    }

    public IStatus validateStartTile() {
        TileSetNode tileSet = getTileSetNode();
        if (tileSet != null && tileSet.getLoadedImage() != null) {
            int tileCount = tileSet.calculateTileCount();
            if (this.startTile > tileCount) {
                return new Status(IStatus.ERROR, Activator.PLUGIN_ID, NLS.bind(Messages.AnimationNode_startTile_INVALID, tileCount));
            }
        }
        return Status.OK_STATUS;
    }

    public TileSetNode getTileSetNode() {
        if (getParent() != null) {
            return (TileSetNode)getParent();
        }
        return null;
    }

    public int getEndTile() {
        return this.endTile;
    }

    public void setEndTile(int endTile) {
        this.endTile = endTile;
    }

    public IStatus validateEndTile() {
        TileSetNode tileSet = getTileSetNode();
        if (tileSet != null && tileSet.getLoadedImage() != null) {
            int tileCount = tileSet.calculateTileCount();
            if (this.endTile > tileCount) {
                return new Status(IStatus.ERROR, Activator.PLUGIN_ID, NLS.bind(Messages.AnimationNode_endTile_INVALID, tileCount));
            }
        }
        return Status.OK_STATUS;
    }

    public Tile.Playback getPlayback() {
        return this.playback;
    }

    public void setPlayback(Tile.Playback playback) {
        this.playback = playback;
    }

    public int getFps() {
        return this.fps;
    }

    public void setFps(int fps) {
        this.fps = fps;
    }

    public boolean isFlipHorizontally() {
        return this.flipHorizontally;
    }

    public void setFlipHorizontally(boolean flipHorizontally) {
        this.flipHorizontally = flipHorizontally;
    }

    public boolean isFlipVertically() {
        return this.flipVertically;
    }

    public void setFlipVertically(boolean flipVertically) {
        this.flipVertically = flipVertically;
    }

    public int getCurrentTile() {
        return this.currentTile;
    }

    public int getTileCount() {
        int tileCount = this.endTile - this.startTile + 1;
        if (this.playback == Playback.PLAYBACK_ONCE_PINGPONG || this.playback == Playback.PLAYBACK_LOOP_PINGPONG) {
            tileCount = Math.max(1, 2 * tileCount - 2);
        }
        return tileCount;
    }

    public void setCursor(float cursor) {
        this.cursor = cursor;
        if (this.playback != Playback.PLAYBACK_NONE) {
            int tileCount = getTileCount();
            float duration = tileCount / (float) this.fps;
            float t = cursor / duration;
            boolean once = this.playback == Playback.PLAYBACK_ONCE_FORWARD
                    || this.playback == Playback.PLAYBACK_ONCE_BACKWARD
                    || this.playback == Playback.PLAYBACK_ONCE_PINGPONG;
            if (once) {
                t = Math.max(0.0f, Math.min(1.0f, t));
            } else {
                int lap = (int) Math.floor(t);
                t -= lap;
            }
            boolean backwards = this.playback == Playback.PLAYBACK_ONCE_BACKWARD
                    || this.playback == Playback.PLAYBACK_LOOP_BACKWARD;
            if (backwards) {
                t = 1.0f - t;
            }
            int tile = (int) Math.min(tileCount - 1, Math.floor(t * tileCount));
            if (this.playback == Playback.PLAYBACK_ONCE_PINGPONG || this.playback == Playback.PLAYBACK_LOOP_PINGPONG) {
                int interval = Math.max(1, tileCount / 2);
                if (tile > interval) {
                    tile = 2 * interval - tile;
                }
            }
            this.currentTile = this.startTile + tile;
        } else {
            this.currentTile = this.startTile;
        }
    }

    public boolean hasFinished() {
        boolean once = this.playback == Playback.PLAYBACK_ONCE_FORWARD
                || this.playback == Playback.PLAYBACK_ONCE_BACKWARD || this.playback == Playback.PLAYBACK_ONCE_PINGPONG;
        if (once) {
            int tileCount = getTileCount();
            float duration = tileCount / (float) this.fps;
            return this.cursor >= duration;
        } else if (this.playback == Playback.PLAYBACK_NONE) {
            return true;
        }
        return false;
    }

    public boolean isPlaying() {
        return this.playing;
    }

    public void setPlaying(boolean playing) {
        this.playing = playing;
    }

    @Override
    public String toString() {
        return this.id;
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getImageRegistry().get(Activator.ANIMATION_IMAGE_ID);
    }
}
