package com.dynamo.cr.tileeditor.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import com.dynamo.cr.tileeditor.scene.AnimationNode;
import com.dynamo.tile.proto.Tile.Playback;
public class AnimationTest {

    @Test
    public void testPlaybackNone() {
        AnimationNode node = new AnimationNode();
        node.setStartTile(1);
        node.setEndTile(4);
        node.setPlayback(Playback.PLAYBACK_NONE);
        node.setFps(30);

        node.setCursor(0.0f);
        assertThat(node.getCurrentTile(), is(1));

        // Overflow
        node.setCursor(-1.0f);
        assertThat(node.getCurrentTile(), is(1));

        // Underflow
        node.setCursor(1.0f);
        assertThat(node.getCurrentTile(), is(1));

        assertTrue(node.hasFinished());
    }

    @Test
    public void testPlaybackOnceForward() {
        AnimationNode node = new AnimationNode();
        node.setStartTile(1);
        node.setEndTile(4);
        node.setPlayback(Playback.PLAYBACK_ONCE_FORWARD);
        node.setFps(30);

        float dt = 1.0f/30;

        // Test boundaries
        node.setCursor(0*dt);
        assertThat(node.getCurrentTile(), is(1));
        node.setCursor(4*dt);
        assertThat(node.getCurrentTile(), is(4));
        assertTrue(node.hasFinished());

        // Test stepping
        float cursor = 0.5f * dt;
        for (int i = 0; i < 4; ++i) {
            node.setCursor(cursor);
            assertThat(node.getCurrentTile(), is(i+1));
            cursor += dt;
        }
        assertFalse(node.hasFinished());

        // Overflow
        node.setCursor(5.0f*dt);
        assertThat(node.getCurrentTile(), is(4));
        assertTrue(node.hasFinished());

        // Underflow
        node.setCursor(-1*dt);
        assertThat(node.getCurrentTile(), is(1));
    }

    @Test
    public void testPlaybackOnceBackward() {
        AnimationNode node = new AnimationNode();
        node.setStartTile(1);
        node.setEndTile(4);
        node.setPlayback(Playback.PLAYBACK_ONCE_BACKWARD);
        node.setFps(30);

        float dt = 1.0f/30;

        // Test boundaries
        node.setCursor(0*dt);
        assertThat(node.getCurrentTile(), is(4));
        node.setCursor(4*dt);
        assertThat(node.getCurrentTile(), is(1));
        assertTrue(node.hasFinished());

        // Test stepping
        float cursor = 0.5f * dt;
        for (int i = 0; i < 4; ++i) {
            node.setCursor(cursor);
            assertThat(node.getCurrentTile(), is(4-i));
            cursor += dt;
        }
        assertFalse(node.hasFinished());

        // Overflow
        node.setCursor(5.0f*dt);
        assertThat(node.getCurrentTile(), is(1));
        assertTrue(node.hasFinished());

        // Underflow
        node.setCursor(-1*dt);
        assertThat(node.getCurrentTile(), is(4));
    }

    @Test
    public void testPlaybackOncePingPong() {
        AnimationNode node = new AnimationNode();
        node.setStartTile(1);
        node.setEndTile(4);
        node.setPlayback(Playback.PLAYBACK_ONCE_PINGPONG);
        node.setFps(30);

        float dt = 1.0f / 30;

        // Test boundaries
        node.setCursor(0 * dt);
        assertThat(node.getCurrentTile(), is(1));
        node.setCursor(6 * dt);
        assertThat(node.getCurrentTile(), is(2));
        assertTrue(node.hasFinished());

        // Test stepping
        float cursor = 0.5f * dt;
        for (int i = 0; i < 4; ++i) {
            node.setCursor(cursor);
            assertThat(node.getCurrentTile(), is(i + 1));
            cursor += dt;
        }
        for (int i = 0; i < 2; ++i) {
            node.setCursor(cursor);
            assertThat(node.getCurrentTile(), is(3 - i));
            cursor += dt;
        }
        assertFalse(node.hasFinished());

        // Overflow
        node.setCursor(7.0f * dt);
        assertThat(node.getCurrentTile(), is(2));
        assertTrue(node.hasFinished());

        // Underflow
        node.setCursor(-1 * dt);
        assertThat(node.getCurrentTile(), is(1));
    }

    @Test
    public void testPlaybackLoopForward() {
        AnimationNode node = new AnimationNode();
        node.setStartTile(1);
        node.setEndTile(4);
        node.setPlayback(Playback.PLAYBACK_LOOP_FORWARD);
        node.setFps(30);

        float dt = 1.0f/30;

        // Test stepping
        float cursor = 0.5f * dt;
        for (int i = 0; i < 8; ++i) {
            node.setCursor(cursor);
            assertThat(node.getCurrentTile(), is(i%4+1));
            cursor += dt;
        }
        assertFalse(node.hasFinished());
    }

    @Test
    public void testPlaybackLoopBackward() {
        AnimationNode node = new AnimationNode();
        node.setStartTile(1);
        node.setEndTile(4);
        node.setPlayback(Playback.PLAYBACK_LOOP_BACKWARD);
        node.setFps(30);

        float dt = 1.0f/30;

        // Test stepping
        float cursor = 0.5f * dt;
        for (int i = 0; i < 8; ++i) {
            node.setCursor(cursor);
            assertThat(node.getCurrentTile(), is(4-i%4));
            cursor += dt;
        }
        assertFalse(node.hasFinished());
    }

    @Test
    public void testPlaybackLoopPingPong() {
        AnimationNode node = new AnimationNode();
        node.setStartTile(1);
        node.setEndTile(4);
        node.setPlayback(Playback.PLAYBACK_LOOP_PINGPONG);
        node.setFps(30);

        float dt = 1.0f/30;

        // Test stepping
        float cursor = 0.5f * dt;
        int tot = 6;
        for (int i = 0; i < 16; ++i) {
            node.setCursor(cursor);
            int mod = i%tot;
            if (mod < 4) {
                ++mod;
            } else {
                mod = tot - mod + 1;
            }
            assertThat(node.getCurrentTile(), is(mod));
            cursor += dt;
        }
        assertFalse(node.hasFinished());
    }

}
