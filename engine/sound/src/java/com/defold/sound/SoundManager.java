package com.defold.sound;

import android.app.Activity;
import android.content.Context;
import android.media.AudioManager;
import android.telephony.PhoneStateListener;
import android.telephony.TelephonyManager;
import android.content.pm.PackageManager;
import android.util.Log;

public class SoundManager {

    public static final String TAG = "defold.sound";

    public native void setPhoneCallState(int active);

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

    private TelephonyManager telephonyManager = null;
    private AudioManager audioManager = null;

    public SoundManager(Activity activity) {
        try {
            this.audioManager = (AudioManager) activity.getSystemService(Context.AUDIO_SERVICE);
            this.telephonyManager = (TelephonyManager) activity.getSystemService(Context.TELEPHONY_SERVICE);
            // DEF-3737
            // We no longer acquire audio focus since that kills any currently playing music which is not
            // what you want if your game just plays sound effects. And the android API only has audio focus
            // for music (the closest type available).

            // DEF-3316
            // On older devices (v5 and below) with apps not specifying the READ_PHONE_STATE permission, the call to
            // SoundManager.this.telephonyManager.listen will trigger a crash.
            // To avoid this we manually check if we have the permission and only register the listener below if the
            // permission is available. However, can't react to incoming phone calls ringing. As soon as the call is
            // picked up Android will switch out the app and silence it.
            // It works only < Android 6. Starting Android 6 user have to request permission READ_PHONE_STATE manually.
            if (activity.checkCallingOrSelfPermission("android.permission.READ_PHONE_STATE") != PackageManager.PERMISSION_GRANTED) {
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

    public boolean isMusicPlaying() {
        try {
            if (this.audioManager != null) {
                if (this.audioManager.isMusicActive()) {
                    return true;
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

}
