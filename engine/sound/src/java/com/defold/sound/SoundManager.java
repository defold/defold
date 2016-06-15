package com.defold.sound;

import android.content.Context;
import android.media.AudioManager;
import android.telephony.TelephonyManager;
import android.util.Log;

public class SoundManager {

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

    private SoundManager.AudioFocusListener listener = null;
    private TelephonyManager telephonyManager = null;
    private AudioManager audioManager = null;

    public SoundManager(Context context) {
        try {
            this.audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
            this.telephonyManager = (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
            this.listener = new SoundManager.AudioFocusListener();
        } catch (Exception e) {
            Log.e("Sound", "An exception occurred while creating AudioManager", e);
        }
    }

    public boolean acquireAudioFocus() {
        try {
            if (this.audioManager != null && this.listener != null) {
                if (!this.listener.hasAudioFocus()) {
                    int result = this.audioManager.requestAudioFocus(this.listener,
                        AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
                    if (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                        this.listener.setAudioFocus(true);
                        return true;
                    } else {
                        this.listener.setAudioFocus(false);
                    }
                }
            }
        } catch (Exception e) {
            Log.e("Sound", "An exception occurred while aquiring audio focus", e);
        }

        return false;
    }

    public boolean isMusicPlaying() {
        try {
            if (this.audioManager != null && this.listener != null) {
                if (this.audioManager.isMusicActive()) {
                    // Sound is playing on the device
                    if (this.listener.hasAudioFocus()) {
                        // If everyone is respecting audio focus, our application
                        // should be responsible for the sound.
                        return false;
                    } else {
                        // Another application is responsible for the sound.
                        return true;
                    }
                } else {
                    // No sound is playing on the device.
                    return false;
                }
            }
        } catch (Exception e) {
            Log.e("Sound", "An exception occurred while checking if music is playing", e);
        }

        // Assume music is playing in case something goes wrong.
        return true;
    }

    public boolean isPhonePlaying() {
        if (this.telephonyManager != null) {
            int callState = this.telephonyManager.getCallState();
            if (callState == TelephonyManager.CALL_STATE_RINGING
                || callState == TelephonyManager.CALL_STATE_OFFHOOK) {
                return true;
            }
        }

        return false;
    }

    public boolean releaseAudioFocus() {
        try {
            if (this.audioManager != null && this.listener != null) {
                if (this.listener.hasAudioFocus()) {
                    int result = this.audioManager.abandonAudioFocus(this.listener);
                    if (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                        this.listener.setAudioFocus(false);
                        return true;
                    }
                }
            }
        } catch (Exception e) {
            Log.e("Sound", "An exception occurred while releasing audio focus", e);
        }

        return false;
    }

}