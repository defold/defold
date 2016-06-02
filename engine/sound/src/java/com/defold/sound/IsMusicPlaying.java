package com.defold.sound;

import android.content.Context;
import android.media.AudioManager;
import android.os.Build;
import android.util.Log;

public class IsMusicPlaying {

    private class AudioFocusListener implements AudioManager.OnAudioFocusChangeListener {

        public void onAudioFocusChange(int focusChange) {

        }

    };

    public static boolean isMusicPlaying(Context context) {
        return false;
    }

}