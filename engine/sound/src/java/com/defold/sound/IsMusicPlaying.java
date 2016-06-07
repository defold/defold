package com.defold.sound;

import android.content.Context;
import android.media.AudioManager;
import android.os.Build;
import android.util.Log;

public class IsMusicPlaying {

    private static class AudioFocusListener implements AudioManager.OnAudioFocusChangeListener {

        private boolean audioFocus = false;

        @Override
        public void onAudioFocusChange(int focusChange) {
            if (focusChange == AudioManager.AUDIOFOCUS_GAIN) {
                this.audioFocus = true;
            } else {
                // AUDIOFOCUS_LOSS
                // AUDIOFOCUS_LOSS_TRANSIENT
                // AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK
                this.audioFocus = false;
            }
        }

        public void setAudioFocus(boolean val) {
            audioFocus = val;
        }

        public boolean hasAudioFocus() {
            return audioFocus;
        }

    };

    private IsMusicPlaying.AudioFocusListener listener = null;
    private AudioManager audioManager = null;

    public IsMusicPlaying(Context context) {
        this.audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        this.listener = new IsMusicPlaying.AudioFocusListener();
    }

    public boolean acquireAudioFocus() {
        if (this.audioManager != null && this.listener != null) {
            if (!this.listener.hasAudioFocus()) {
                int result = this.audioManager.requestAudioFocus(this.listener, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
                if (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                    this.listener.setAudioFocus(true);
                    return true;
                } else {
                    this.listener.setAudioFocus(false);
                }
            }
        }

        return false;
    }

    public boolean isMusicPlaying() {
        if (this.audioManager != null && this.listener != null) {
            if (this.audioManager.isMusicActive()) {
                // Sound is playing on the device
                if (this.listener.hasAudioFocus()) {
                    // Our application is responsible for the sound
                    return false;
                } else {
                    // Another application is responsible for the sound
                    return true;
                }
            } else {
                // No sound is playing on the device
                return false;
            }
        }

        return true;
    }

    public boolean releaseAudioFocus() {
        if (this.audioManager != null && this.listener != null) {
            if (this.listener.hasAudioFocus()) {
                int result = this.audioManager.abandonAudioFocus(this.listener);
                if (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                    this.listener.setAudioFocus(false);
                    return true;
                }
            }
        }

        return false;
    }

}