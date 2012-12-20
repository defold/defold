package com.dynamo.cr.tileeditor.scene;

import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.properties.GreaterThanZero;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Identifiable;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.Activator;
import com.dynamo.tile.proto.Tile.Playback;

@SuppressWarnings("serial")
public class AtlasAnimationNode extends Node implements Identifiable {
    @Property
    private String id;
    @Property
    private Playback playback = Playback.PLAYBACK_ONCE_FORWARD;
    @GreaterThanZero
    @Property
    private int fps = 30;
    @Property
    private boolean flipHorizontally;
    @Property
    private boolean flipVertically;

    public AtlasAnimationNode() {
        this.id = "anim";
    }

    public void setId(String id) {
        this.id = id;
    }

    public String getId() {
        return id;
    }

    @Override
    protected void childAdded(Node child) {
        super.childAdded(child);
        if (getParent() != null)
            ((AtlasNode) getParent()).increaseVersion();
    }

    @Override
    protected void childRemoved(Node child) {
        super.childRemoved(child);
        if (getParent() != null)
            ((AtlasNode) getParent()).increaseVersion();
    }

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

    @Override
    public String toString() {
        return id;
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getImageRegistry().get(Activator.ANIMATION_IMAGE_ID);
    }

}
