package com.dynamo.cr.tileeditor.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.eclipse.jface.viewers.StructuredSelection;
import org.junit.Before;
import org.junit.Test;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.tileeditor.scene.AnimationNode;
import com.dynamo.cr.tileeditor.util.Animator;
import com.dynamo.tile.proto.Tile.Playback;

public class AnimatorTest {

    private IPresenterContext presenterContext;
    private Animator animator;
    private AnimationNode node;

    private int timeout = 0;
    private int timer = 0;
    private int refreshCounter;

    @Before
    public void setup() {
        // Mocking, bounded execution
        this.presenterContext = mock(IPresenterContext.class);
        this.timeout = 10; // 10 frames timeout
        doAnswer(new Answer<Object>() {
            private long time = 0;

            @Override
            public Object answer(InvocationOnMock invocation) throws Throwable {
                if (timer < timeout) {
                    // Rough vsync
                    long time = System.currentTimeMillis();
                    long delta = timer == 0 ? 0 : time - this.time;
                    if (delta < 16) {
                        Thread.sleep(16 - delta);
                    }
                    this.time = System.currentTimeMillis();
                    // Timeout-timer
                    ++timer;
                    // Execute
                    Runnable runnable = (Runnable)invocation.getArguments()[0];
                    runnable.run();
                }
                return null;
            }
        }).when(presenterContext).asyncExec(any(Runnable.class));

        this.animator = new Animator();

        this.node = new AnimationNode();
        this.node.setStartTile(1);
        this.node.setEndTile(4);
        this.node.setFps(30);

        this.refreshCounter = 0;
    }

    private void select() {
        when(this.presenterContext.getSelection()).thenReturn(new StructuredSelection(this.node));
    }

    private void selectNone() {
        when(this.presenterContext.getSelection()).thenReturn(new StructuredSelection());
    }

    private void verifyRefreshAtLeast(int required) {
        this.refreshCounter += required;
        verify(this.presenterContext, atLeast(this.refreshCounter)).refreshRenderView();
    }

    @Test
    public void testStartOnce() {
        this.node.setPlayback(Playback.PLAYBACK_ONCE_FORWARD);

        select();
        assertFalse(this.node.hasFinished());
        this.animator.start(this.presenterContext);
        assertTrue(this.node.hasFinished());
        assertFalse(this.animator.isRunning());
        verifyRefreshAtLeast(1);
    }

    @Test
    public void testStartLoop() {
        this.node.setPlayback(Playback.PLAYBACK_LOOP_FORWARD);

        select();
        assertFalse(this.node.hasFinished());
        this.animator.start(this.presenterContext);
        assertFalse(this.node.hasFinished());
        assertTrue(this.animator.isRunning());
        verifyRefreshAtLeast(1);
    }

    @Test
    public void testStopLoop() {
        testStartLoop();

        this.animator.stop();
        assertFalse(this.animator.isRunning());
    }

    @Test
    public void testStopBySelection() {
        testStartLoop();

        assertTrue(this.animator.isRunning());

        selectNone();
        // Simulate one more async run
        this.animator.run();
        assertFalse(this.animator.isRunning());
    }

}
