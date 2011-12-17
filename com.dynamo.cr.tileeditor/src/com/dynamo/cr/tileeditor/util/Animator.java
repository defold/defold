package com.dynamo.cr.tileeditor.util;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.tileeditor.scene.AnimationNode;
import com.dynamo.cr.tileeditor.scene.TileSetUtil;

/**
 * Progresses an animation by moving it's cursor through continuous calls to IPresenterContext#asyncExec.
 *
 * NOTE This class should only be run in the UI thread, i.e. through calling start().
 */
public class Animator implements Runnable {

    private IPresenterContext presenterContext;
    private AnimationNode animationNode;
    private boolean running;
    private long startTime;

    public Animator() {
        this.presenterContext = null;
        this.animationNode = null;
        this.running = false;
    }

    public void start(IPresenterContext presenterContext) {
        this.presenterContext = presenterContext;
        if (!this.running) {
            AnimationNode node = TileSetUtil.getCurrentAnimation(this.presenterContext.getSelection());
            if (node != null) {
                this.animationNode = node;
                this.animationNode.setPlaying(true);
                this.running = true;
                this.startTime = System.currentTimeMillis();
                // NOTE this call is not async during test, so setup before
                this.presenterContext.asyncExec(this);
            } else {
                this.animationNode = null;
            }
        }
    }

    public void stop() {
        this.running = false;
        this.animationNode.setPlaying(false);
    }

    public boolean isRunning() {
        return this.running;
    }

    @Override
    public void run() {
        if (this.running) {
            AnimationNode node = TileSetUtil.getCurrentAnimation(this.presenterContext.getSelection());
            if (this.animationNode != node || !this.animationNode.isPlaying()) {
                this.running = false;
                return;
            }
            long time = System.currentTimeMillis();
            long delta = time - this.startTime;
            float cursor = (float)(delta / 1000.0);
            this.animationNode.setCursor(cursor);
            this.presenterContext.refreshRenderView();
            if (this.animationNode.hasFinished()) {
                this.running = false;
                this.animationNode.setPlaying(false);
            } else {
                // NOTE this call is not async during tests
                this.presenterContext.asyncExec(this);
            }
        }
    }

}
