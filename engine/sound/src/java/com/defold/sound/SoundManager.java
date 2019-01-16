package com.defold.sound;

import android.app.Activity;
import android.content.Context;
import android.media.AudioManager;
import android.telephony.PhoneStateListener;
import android.telephony.TelephonyManager;
import android.support.v4.content.ContextCompat;
import android.Manifest;
import android.content.pm.PackageManager;
import android.util.Log;

public class SoundManager {

    public static final String TAG = "defold.sound";
    
    public native void setPhoneCallState(int active);

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

    private static class CustomPhoneCallListener extends PhoneStateListener {
        private SoundManager manager = null;

        public CustomPhoneCallListener(SoundManager manager) {
            this.manager = manager;
        }

        @Override
        public void onCallStateChanged(int state, String incomingNumber) {
            super.onCallStateChanged(state, incomingNumber);
            manager.updatePhoneCallState(state);
        }
    }

    private SoundManager.AudioFocusListener listener = null;
    private TelephonyManager telephonyManager = null;
    private AudioManager audioManager = null;

    public SoundManager(Activity activity) {
        try {
            this.audioManager = (AudioManager) activity.getSystemService(Context.AUDIO_SERVICE);
            this.telephonyManager = (TelephonyManager) activity.getSystemService(Context.TELEPHONY_SERVICE);
            this.listener = new SoundManager.AudioFocusListener();

            // DEF-3316
            // On older devices (v5 and below) with apps not specifying the READ_PHONE_STATE permission, the call to
            // SoundManager.this.telephonyManager.listen will trigger a crash.
            // To avoid this we manually check if we have the permission and only register the listener below if the
            // permission is available. However, this means that the app will continue playing audio even if there is a phone call.
            if (ContextCompat.checkSelfPermission(activity, Manifest.permission.READ_PHONE_STATE) != PackageManager.PERMISSION_GRANTED) {
                Log.e(TAG, "App is missing the READ_PHONE_STATE permission. Audio will continue while phone call is active.");
            } else {
                activity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        SoundManager.this.telephonyManager.listen(new CustomPhoneCallListener(SoundManager.this), PhoneStateListener.LISTEN_CALL_STATE);
                    }
                });
            }

            this.updatePhoneCallState(this.telephonyManager.getCallState());
        } catch (Exception e) {
            Log.e(TAG, "An exception occurred while retrieving system services", e);
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
            Log.e(TAG, "An exception occurred while aquiring audio focus", e);
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
            Log.e(TAG, "An exception occurred while checking if music is playing", e);
        }

        // Assume music is playing in case something goes wrong.
        return true;
    }

    public static boolean isPhoneCallActive(int state) {
        return state == TelephonyManager.CALL_STATE_RINGING
            || state == TelephonyManager.CALL_STATE_OFFHOOK;
    }

    public void updatePhoneCallState(int state) {
        this.setPhoneCallState(isPhoneCallActive(state) ? 1 : 0);
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
            Log.e(TAG, "An exception occurred while releasing audio focus", e);
        }

        return false;
    }

}
